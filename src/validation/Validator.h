#pragma once
#include <QString>
#include <QStringList>
#include <QList>

class Network;

struct ValidationIssue {
    enum class Severity { Error, Warning, Info };

    Severity    severity;
    QString     message;
    QStringList deviceIds; // IDs of affected devices (may be empty)

    QString severityString() const {
        switch (severity) {
            case Severity::Error:   return "ERROR";
            case Severity::Warning: return "WARNING";
            case Severity::Info:    return "INFO";
        }
        return {};
    }
};

class Validator
{
public:
    static QList<ValidationIssue> validate(Network *network);

private:
    static void checkIpConflicts(Network *network, QList<ValidationIssue> &issues);
    static void checkSubnetMismatches(Network *network, QList<ValidationIssue> &issues);
    static void checkPcGateways(Network *network, QList<ValidationIssue> &issues);
    static void checkOspfRouterIds(Network *network, QList<ValidationIssue> &issues);
    static void checkUnconnectedInterfaces(Network *network, QList<ValidationIssue> &issues);
    static void checkRipNetworks(Network *network, QList<ValidationIssue> &issues);
    static void checkReachability(Network *network, QList<ValidationIssue> &issues);
};
