#include "routing/StaticRouting.h"
#include "models/Network.h"
#include "utils/IpUtils.h"

void StaticRouting::compute(Network *network)
{
    for (auto *router : network->routers()) {
        if (router->routingProtocol() != Router::RoutingProtocol::Static) continue;
        router->clearRoutingTable();

        // Directly-connected networks
        for (const auto &iface : router->interfaces()) {
            if (!iface.isConfigured()) continue;
            RoutingEntry e;
            e.destination  = IpUtils::format(iface.networkAddr());
            e.mask         = iface.subnetMask;
            e.nextHop      = "directly connected";
            e.exitInterface = iface.name;
            e.metric       = 0;
            e.protocol     = "Connected";
            router->addRoutingEntry(e);
        }

        // User-defined static routes
        for (const auto &sr : router->staticRoutes()) {
            if (sr.destination.isEmpty() || sr.mask.isEmpty()) continue;
            RoutingEntry e;
            e.destination  = sr.destination;
            e.mask         = sr.mask;
            e.nextHop      = sr.nextHop;
            e.metric       = sr.metric;
            e.protocol     = "Static";

            // Determine exit interface by matching next-hop to a connected subnet
            for (const auto &iface : router->interfaces()) {
                if (!iface.isConfigured()) continue;
                if (IpUtils::sameSubnet(sr.nextHop, iface.ipAddress, iface.subnetMask)) {
                    e.exitInterface = iface.name;
                    break;
                }
            }
            router->addRoutingEntry(e);
        }
    }
}
