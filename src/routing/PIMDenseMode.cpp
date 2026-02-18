#include "routing/PIMDenseMode.h"
#include "models/Network.h"
#include "utils/IpUtils.h"
#include <QHash>
#include <QQueue>
#include <climits>

// ---------------------------------------------------------------------------
// Build a shortest-path tree (SPT) from 'sourceIp' across all PIM-DM routers
// using a simple BFS weighted by hop count. This represents the initial flood.
// Then prune branches that lead only to routers with no active receivers
// (for simplicity: routers with no PCs connected downstream are pruned).
// ---------------------------------------------------------------------------

// Returns the router whose interface owns the given IP, or nullptr.
static Router *routerOwningIp(const QString &ip, Network *network)
{
    for (auto *r : network->routers())
        for (const auto &iface : r->interfaces())
            if (iface.ipAddress == ip) return r;
    return nullptr;
}

// Determine the first-hop PIM-DM router reachable from sourceIp.
static Router *findFirstHopRouter(const QString &sourceIp, Network *network)
{
    // Check if a router owns this IP directly
    if (Router *r = routerOwningIp(sourceIp, network))
        return r;

    // Look for a PC with this IP and find a router connected via switch/hub/direct link
    PC *srcPc = nullptr;
    for (auto *pc : network->pcs())
        if (pc->ipAddress() == sourceIp) { srcPc = pc; break; }
    if (!srcPc) return nullptr;

    // Walk links from the PC toward a router (may pass through switch/hub)
    QSet<QString> visited;
    QQueue<QString> queue;
    queue.enqueue(srcPc->id());

    while (!queue.isEmpty()) {
        const QString current = queue.dequeue();
        if (visited.contains(current)) continue;
        visited.insert(current);

        for (const Link *link : network->linksForDevice(current)) {
            Device *nbr = network->neighbor(link, current);
            if (!nbr) continue;
            if (Router *r = qobject_cast<Router *>(nbr))
                return r;
            queue.enqueue(nbr->id());
        }
    }
    return nullptr;
}

// Returns true if the sub-tree rooted at 'routerId' (excluding 'parentId')
// has any PC connected (even through switches/hubs).
static bool hasPcDownstream(const QString &routerId,
                             const QString &parentId,
                             Network *network,
                             QSet<QString> &visited)
{
    if (visited.contains(routerId)) return false;
    visited.insert(routerId);

    for (const Link *link : network->linksForDevice(routerId)) {
        Device *nbr = network->neighbor(link, routerId);
        if (!nbr || nbr->id() == parentId) continue;

        if (qobject_cast<PC *>(nbr)) return true;

        // Switch or Hub – keep searching
        if (qobject_cast<Switch *>(nbr) || qobject_cast<Hub *>(nbr)) {
            if (hasPcDownstream(nbr->id(), routerId, network, visited)) return true;
        }

        // Another PIM-DM router
        if (Router *r = qobject_cast<Router *>(nbr)) {
            if (r->routingProtocol() == Router::RoutingProtocol::PIM_DM) {
                if (hasPcDownstream(r->id(), routerId, network, visited)) return true;
            }
        }
    }
    return false;
}

MulticastTree PIMDenseMode::compute(Network *network,
                                    const QString &sourceIp,
                                    const QString &groupAddr)
{
    MulticastTree tree;
    tree.sourceIp    = sourceIp;
    tree.groupAddress = groupAddr;

    Router *firstHop = findFirstHopRouter(sourceIp, network);
    if (!firstHop) return tree;

    // -----------------------------------------------------------------------
    // BFS flood from firstHop across PIM-DM routers only
    // -----------------------------------------------------------------------
    struct BfsNode {
        QString routerId;
        QString parentId;
        QString inIface;     // interface used to receive multicast on this router
    };

    QQueue<BfsNode> queue;
    QSet<QString>   visited;
    queue.enqueue({firstHop->id(), QString(), QString()});

    QList<BfsNode> flood; // order of flooding

    while (!queue.isEmpty()) {
        BfsNode node = queue.dequeue();
        if (visited.contains(node.routerId)) continue;
        visited.insert(node.routerId);
        flood.append(node);

        Router *r = qobject_cast<Router *>(network->device(node.routerId));
        if (!r) continue;

        for (const Link *link : network->linksForDevice(node.routerId)) {
            Device *nbr = network->neighbor(link, node.routerId);
            Router *nbrRouter = qobject_cast<Router *>(nbr);
            if (!nbrRouter) continue;
            if (nbrRouter->routingProtocol() != Router::RoutingProtocol::PIM_DM) continue;
            if (visited.contains(nbrRouter->id())) continue;

            const QString nbrInIface = network->interfaceForLink(link, nbrRouter->id());
            queue.enqueue({nbrRouter->id(), node.routerId, nbrInIface});
        }
    }

    // -----------------------------------------------------------------------
    // For each flooded router, determine OIL and prune leaves without PCs
    // -----------------------------------------------------------------------
    for (const BfsNode &node : flood) {
        Router *r = qobject_cast<Router *>(network->device(node.routerId));
        if (!r) continue;

        // Gather all outgoing interfaces (toward downstream PIM-DM routers)
        QStringList oil;
        bool hasDownstreamPc = false;

        for (const Link *link : network->linksForDevice(node.routerId)) {
            Device *nbr = network->neighbor(link, node.routerId);
            Router *nbrRouter = qobject_cast<Router *>(nbr);

            // Check for directly-connected PCs (through this router)
            if (qobject_cast<PC *>(nbr)) { hasDownstreamPc = true; continue; }

            // Non-PIM-DM or back-toward-parent: skip
            if (!nbrRouter) {
                // Switch/hub – check if any PC is downstream
                if (qobject_cast<Switch *>(nbr) || qobject_cast<Hub *>(nbr)) {
                    QSet<QString> vis2;
                    if (hasPcDownstream(nbr->id(), node.routerId, network, vis2)) {
                        hasDownstreamPc = true;
                        const QString oif = network->interfaceForLink(link, node.routerId);
                        oil.append(oif);
                    }
                }
                continue;
            }
            if (nbrRouter->routingProtocol() != Router::RoutingProtocol::PIM_DM) continue;
            if (nbrRouter->id() == node.parentId) continue; // RPF interface

            // Downstream PIM-DM router – include in OIL only if it has receivers
            QSet<QString> vis2;
            if (hasPcDownstream(nbrRouter->id(), node.routerId, network, vis2)) {
                const QString oif = network->interfaceForLink(link, node.routerId);
                oil.append(oif);
            } else {
                tree.pruned.append(nbrRouter->name());
            }
        }

        if (!hasDownstreamPc && oil.isEmpty() && node.routerId != firstHop->id()) {
            tree.pruned.append(r->name());
            continue; // this router is pruned
        }

        MulticastTreeEntry entry;
        entry.routerName          = r->name();
        entry.routerId            = r->id();
        entry.incomingInterface   = node.inIface;
        entry.outgoingInterfaces  = oil;
        tree.entries.append(entry);
    }

    return tree;
}
