#pragma once
class Network;

class RIPv2
{
public:
    // Populates computedRoutingTable on every RIPv2 router in the network.
    static void compute(Network *network);
};
