#pragma once
#include <QString>
#include <QList>
#include "models/Device.h"
#include "routing/PIMDenseMode.h"

class Network;

struct RouterSimResult {
    QString             routerId;
    QString             routerName;
    QString             protocol;
    QList<RoutingEntry> routingTable;
    QStringList         warnings;
};

struct SimulationResult {
    QList<RouterSimResult> routerResults;
    QList<MulticastTree>   multicastTrees; // one per PIM-DM source/group pair
};

class RoutingEngine
{
public:
    // Run all routing protocols and return the aggregated result.
    static SimulationResult run(Network *network,
                                const QString &pimSourceIp  = QString(),
                                const QString &pimGroupAddr = QString());
};
