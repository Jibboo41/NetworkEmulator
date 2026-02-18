#pragma once
class Network;

class OSPF
{
public:
    // Populates computedRoutingTable on every OSPF router in the network
    // using Dijkstra's SPF algorithm.
    static void compute(Network *network);
};
