#include "routing/RoutingEngine.h"
#include "routing/RIPv2.h"
#include "routing/OSPF.h"
#include "routing/StaticRouting.h"
#include "routing/PIMDenseMode.h"
#include "models/Network.h"

SimulationResult RoutingEngine::run(Network *network,
                                    const QString &pimSourceIp,
                                    const QString &pimGroupAddr)
{
    SimulationResult result;

    // Run each unicast protocol
    StaticRouting::compute(network);
    RIPv2::compute(network);
    OSPF::compute(network);
    // PIM-DM routers also get their connected routes via their own compute pass
    // (we reuse the OSPF infrastructure; here we add connected routes manually)
    for (auto *router : network->routers()) {
        if (router->routingProtocol() != Router::RoutingProtocol::PIM_DM) continue;
        router->clearRoutingTable();
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
    }

    // Collect per-router results
    for (auto *router : network->routers()) {
        RouterSimResult rr;
        rr.routerId   = router->id();
        rr.routerName = router->name();
        switch (router->routingProtocol()) {
            case Router::RoutingProtocol::Static: rr.protocol = "Static";        break;
            case Router::RoutingProtocol::RIPv2:  rr.protocol = "RIPv2";         break;
            case Router::RoutingProtocol::OSPF:   rr.protocol = "OSPF";          break;
            case Router::RoutingProtocol::PIM_DM: rr.protocol = "PIM Dense Mode"; break;
        }
        rr.routingTable = router->computedRoutingTable();
        result.routerResults.append(rr);
    }

    // PIM-DM multicast tree (if requested)
    if (!pimSourceIp.isEmpty() && !pimGroupAddr.isEmpty()) {
        MulticastTree tree = PIMDenseMode::compute(network, pimSourceIp, pimGroupAddr);
        result.multicastTrees.append(tree);
    }

    return result;
}
