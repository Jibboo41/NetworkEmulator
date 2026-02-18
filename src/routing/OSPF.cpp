#include "routing/OSPF.h"
#include "models/Network.h"
#include "utils/IpUtils.h"
#include <QHash>
#include <QSet>
#include <climits>

// ---------------------------------------------------------------------------
// Internal edge structure used by Dijkstra
// ---------------------------------------------------------------------------
struct OspfEdge {
    QString neighborId;
    int     cost;
    QString localInterface;   // on the source router
    QString neighborInterface;// on the neighbor router
    QString neighborIp;       // IP of the neighbor's interface (used as next-hop)
};

void OSPF::compute(Network *network)
{
    // -----------------------------------------------------------------------
    // Build adjacency list for OSPF routers
    // -----------------------------------------------------------------------
    QList<Router *> ospfRouters;
    for (auto *r : network->routers())
        if (r->routingProtocol() == Router::RoutingProtocol::OSPF)
            ospfRouters.append(r);

    QHash<QString, QList<OspfEdge>> adjacency; // routerId -> edges

    for (auto *router : ospfRouters) {
        adjacency[router->id()] = {};
        for (const Link *link : network->linksForDevice(router->id())) {
            Router *nbr = qobject_cast<Router *>(network->neighbor(link, router->id()));
            if (!nbr || nbr->routingProtocol() != Router::RoutingProtocol::OSPF) continue;

            const QString localIface = network->interfaceForLink(link, router->id());
            const QString nbrIface   = network->interfaceForLink(link, nbr->id());

            int cost = 1;
            if (const NetworkInterface *iface = router->getInterface(localIface))
                cost = iface->ospfCost;

            QString nbrIp;
            if (const NetworkInterface *iface = nbr->getInterface(nbrIface))
                nbrIp = iface->ipAddress;

            adjacency[router->id()].append({nbr->id(), cost, localIface, nbrIface, nbrIp});
        }
    }

    // -----------------------------------------------------------------------
    // Run Dijkstra from every OSPF router
    // -----------------------------------------------------------------------
    for (auto *root : ospfRouters) {
        root->clearRoutingTable();

        // Add directly-connected networks
        for (const auto &iface : root->interfaces()) {
            if (!iface.isConfigured()) continue;
            RoutingEntry e;
            e.destination  = IpUtils::format(iface.networkAddr());
            e.mask         = iface.subnetMask;
            e.nextHop      = "directly connected";
            e.exitInterface = iface.name;
            e.metric       = 0;
            e.protocol     = "Connected";
            root->addRoutingEntry(e);
        }

        // dist[id] = total cost from root to that router
        QHash<QString, int>     dist;
        QHash<QString, QString> firstHopIp;    // next-hop IP toward this router
        QHash<QString, QString> firstHopIface; // exit interface toward this router

        for (auto *r : ospfRouters)
            dist[r->id()] = (r->id() == root->id()) ? 0 : INT_MAX;

        QSet<QString> visited;

        while (visited.size() < static_cast<int>(ospfRouters.size())) {
            // Pick unvisited node with minimum distance
            QString u;
            int minDist = INT_MAX;
            for (auto *r : ospfRouters) {
                if (!visited.contains(r->id()) && dist[r->id()] < minDist) {
                    minDist = dist[r->id()];
                    u = r->id();
                }
            }
            if (u.isEmpty() || minDist == INT_MAX) break;
            visited.insert(u);

            for (const OspfEdge &edge : adjacency[u]) {
                int newDist = dist[u] + edge.cost;
                if (newDist < dist[edge.neighborId]) {
                    dist[edge.neighborId] = newDist;
                    if (u == root->id()) {
                        // Direct neighbor of root
                        firstHopIp[edge.neighborId]    = edge.neighborIp;
                        firstHopIface[edge.neighborId] = edge.localInterface;
                    } else {
                        // Inherit first-hop from the parent
                        firstHopIp[edge.neighborId]    = firstHopIp.value(u);
                        firstHopIface[edge.neighborId] = firstHopIface.value(u);
                    }
                }
            }
        }

        // Add routes to every reachable OSPF router's networks
        for (auto *other : ospfRouters) {
            if (other->id() == root->id()) continue;
            if (dist[other->id()] == INT_MAX) continue;

            for (const auto &iface : other->interfaces()) {
                if (!iface.isConfigured()) continue;
                const QString dest = IpUtils::format(iface.networkAddr());

                // Don't duplicate an entry already in the table
                bool exists = false;
                for (const auto &existing : root->computedRoutingTable())
                    if (existing.destination == dest && existing.mask == iface.subnetMask)
                        { exists = true; break; }
                if (exists) continue;

                RoutingEntry e;
                e.destination  = dest;
                e.mask         = iface.subnetMask;
                e.nextHop      = firstHopIp.value(other->id(), "unknown");
                e.exitInterface = firstHopIface.value(other->id(), "unknown");
                e.metric       = dist[other->id()];
                e.protocol     = "OSPF";
                root->addRoutingEntry(e);
            }
        }
    }
}
