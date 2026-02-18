#pragma once
class Network;

class StaticRouting
{
public:
    // Populates computedRoutingTable on every Static-protocol router.
    static void compute(Network *network);
};
