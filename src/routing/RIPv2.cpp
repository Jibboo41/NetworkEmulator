#include "routing/RIPv2.h"
#include "models/Network.h"
#include "utils/IpUtils.h"
#include <QHash>
#include <QString>

// Helpers
static QString routeKey(const QString &dest, const QString &mask)
{
    return dest + "/" + mask;
}

static QString ipOnLink(Router *router, const Link *link, Network *network)
{
    const QString ifaceName = network->interfaceForLink(link, router->id());
    if (const NetworkInterface *iface = router->getInterface(ifaceName))
        return iface->ipAddress;
    return {};
}

void RIPv2::compute(Network *network)
{
    // -----------------------------------------------------------------------
    // Step 1: Initialise each RIPv2 router with directly-connected routes.
    // -----------------------------------------------------------------------
    const QList<Router *> allRouters = network->routers();
    QList<Router *> ripRouters;
    for (auto *r : allRouters) {
        if (r->routingProtocol() != Router::RoutingProtocol::RIPv2) continue;
        ripRouters.append(r);
        r->clearRoutingTable();

        for (const auto &iface : r->interfaces()) {
            if (!iface.isConfigured()) continue;
            quint32 net = iface.networkAddr();
            RoutingEntry e;
            e.destination  = IpUtils::format(net);
            e.mask         = iface.subnetMask;
            e.nextHop      = "directly connected";
            e.exitInterface = iface.name;
            e.metric       = 1;
            e.protocol     = "Connected";
            r->addRoutingEntry(e);
        }
    }

    // -----------------------------------------------------------------------
    // Step 2: Bellman-Ford – iterate until convergence or metric 16 reached.
    // -----------------------------------------------------------------------
    // We track for each (router, routeKey) which router it was learned from
    // to apply split horizon.
    QHash<QString /*routerId*/, QHash<QString /*routeKey*/, QString /*learnedFrom*/>> learnedFrom;

    const int MAX_METRIC = 15;
    bool changed = true;

    while (changed) {
        changed = false;

        for (auto *router : ripRouters) {
            const auto links = network->linksForDevice(router->id());
            for (const Link *link : links) {
                Device *neighborDev = network->neighbor(link, router->id());
                Router *neighbor    = qobject_cast<Router *>(neighborDev);
                if (!neighbor || neighbor->routingProtocol() != Router::RoutingProtocol::RIPv2)
                    continue;

                const QString neighborIfaceName = network->interfaceForLink(link, neighbor->id());
                const QString routerIp = ipOnLink(router, link, network);

                // Advertise our routing table to the neighbor
                for (const auto &entry : router->computedRoutingTable()) {
                    const QString key = routeKey(entry.destination, entry.mask);

                    // Split horizon: do not advertise back to where we learned it
                    if (learnedFrom[router->id()].value(key) == neighbor->id())
                        continue;

                    int newMetric = entry.metric + 1;
                    if (newMetric > MAX_METRIC) continue;

                    // Check existing entry in neighbor
                    bool found = false;
                    for (auto &ne : neighbor->computedRoutingTable()) {
                        if (ne.destination == entry.destination && ne.mask == entry.mask) {
                            found = true;
                            if (newMetric < ne.metric) {
                                ne.metric        = newMetric;
                                ne.nextHop       = routerIp;
                                ne.exitInterface = neighborIfaceName;
                                ne.protocol      = "RIPv2";
                                learnedFrom[neighbor->id()][key] = router->id();
                                changed = true;
                            }
                            break;
                        }
                    }
                    if (!found) {
                        RoutingEntry ne;
                        ne.destination  = entry.destination;
                        ne.mask         = entry.mask;
                        ne.nextHop      = routerIp;
                        ne.exitInterface = neighborIfaceName;
                        ne.metric       = newMetric;
                        ne.protocol     = "RIPv2";
                        neighbor->addRoutingEntry(ne);
                        learnedFrom[neighbor->id()][key] = router->id();
                        changed = true;
                    }
                }
            }
        }
    }
}
