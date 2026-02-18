#pragma once
#include <QString>
#include <QList>

class Network;

struct MulticastTreeEntry {
    QString routerName;
    QString routerId;
    QString incomingInterface;  // RPF interface (toward source)
    QStringList outgoingInterfaces; // OIL – toward receivers
};

struct MulticastTree {
    QString sourceIp;
    QString groupAddress;
    QList<MulticastTreeEntry> entries; // one per PIM-DM router on the tree
    QStringList pruned;                // router names that were pruned
};

class PIMDenseMode
{
public:
    // Builds multicast distribution trees for all PIM-DM routers.
    // sourceIp:    IP of the multicast source (typically a PC or loopback)
    // groupAddr:   multicast group address (e.g. 239.1.1.1)
    // Returns one MulticastTree describing the flood-and-prune result.
    static MulticastTree compute(Network *network,
                                 const QString &sourceIp,
                                 const QString &groupAddr);
};
