#include "validation/Validator.h"
#include "models/Network.h"
#include "utils/IpUtils.h"
#include <QSet>
#include <QQueue>

// ---------------------------------------------------------------------------
QList<ValidationIssue> Validator::validate(Network *network)
{
    QList<ValidationIssue> issues;
    checkIpConflicts(network, issues);
    checkSubnetMismatches(network, issues);
    checkPcGateways(network, issues);
    checkOspfRouterIds(network, issues);
    checkUnconnectedInterfaces(network, issues);
    checkRipNetworks(network, issues);
    checkReachability(network, issues);
    return issues;
}

// ---------------------------------------------------------------------------
// Check for duplicate IP addresses across all devices
// ---------------------------------------------------------------------------
void Validator::checkIpConflicts(Network *network, QList<ValidationIssue> &issues)
{
    QHash<QString, QStringList> ipToDevices; // ip -> list of device names

    for (const auto *dev : network->devices()) {
        for (const auto &iface : dev->interfaces()) {
            if (!iface.isConfigured()) continue;
            ipToDevices[iface.ipAddress].append(
                QString("%1 (%2)").arg(dev->name(), iface.name));
        }
    }

    for (auto it = ipToDevices.constBegin(); it != ipToDevices.constEnd(); ++it) {
        if (it.value().size() > 1) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Error;
            issue.message  = QString("IP address conflict: %1 is assigned to: %2")
                                 .arg(it.key(), it.value().join(", "));
            issues.append(issue);
        }
    }
}

// ---------------------------------------------------------------------------
// Check that connected interfaces are on the same subnet
// ---------------------------------------------------------------------------
void Validator::checkSubnetMismatches(Network *network, QList<ValidationIssue> &issues)
{
    for (const Link *link : network->links()) {
        const Device *d1 = network->device(link->device1Id);
        const Device *d2 = network->device(link->device2Id);
        if (!d1 || !d2) continue;

        const NetworkInterface *if1 = d1->getInterface(link->interface1);
        const NetworkInterface *if2 = d2->getInterface(link->interface2);
        if (!if1 || !if2) continue;
        if (!if1->isConfigured() || !if2->isConfigured()) continue;

        // Skip switch/hub ports – they are layer-2 only
        if (d1->deviceType() == Device::Type::Switch || d1->deviceType() == Device::Type::Hub) continue;
        if (d2->deviceType() == Device::Type::Switch || d2->deviceType() == Device::Type::Hub) continue;

        quint32 net1 = if1->networkAddr();
        quint32 net2 = if2->networkAddr();

        if (net1 != net2 || if1->subnetMask != if2->subnetMask) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Error;
            issue.message  = QString("Subnet mismatch on link %1 (%2: %3/%4) <-> %5 (%6: %7/%8)")
                                 .arg(d1->name(), if1->name, if1->ipAddress, if1->subnetMask,
                                      d2->name(), if2->name, if2->ipAddress, if2->subnetMask);
            issue.deviceIds << d1->id() << d2->id();
            issues.append(issue);
        }
    }
}

// ---------------------------------------------------------------------------
// Check that each PC has a valid default gateway
// ---------------------------------------------------------------------------
void Validator::checkPcGateways(Network *network, QList<ValidationIssue> &issues)
{
    for (const auto *dev : network->devices()) {
        const PC *pc = qobject_cast<const PC *>(dev);
        if (!pc) continue;
        if (!pc->interfaces().first().isConfigured()) continue;

        if (pc->defaultGateway().isEmpty()) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Warning;
            issue.message  = QString("PC '%1' has no default gateway configured.").arg(pc->name());
            issue.deviceIds << pc->id();
            issues.append(issue);
            continue;
        }

        // Gateway must be on the same subnet
        const NetworkInterface &eth = pc->interfaces().first();
        if (!IpUtils::sameSubnet(pc->ipAddress(), pc->defaultGateway(), eth.subnetMask)) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Error;
            issue.message  = QString("PC '%1': default gateway %2 is not on the same subnet as %3/%4.")
                                 .arg(pc->name(), pc->defaultGateway(),
                                      pc->ipAddress(), eth.subnetMask);
            issue.deviceIds << pc->id();
            issues.append(issue);
        }
    }
}

// ---------------------------------------------------------------------------
// Check for duplicate OSPF router IDs
// ---------------------------------------------------------------------------
void Validator::checkOspfRouterIds(Network *network, QList<ValidationIssue> &issues)
{
    QHash<QString, QStringList> ridToRouters;

    for (auto *router : network->routers()) {
        if (router->routingProtocol() != Router::RoutingProtocol::OSPF) continue;
        const QString rid = router->ospfConfig().routerId;
        if (!rid.isEmpty())
            ridToRouters[rid].append(router->name());
    }

    for (auto it = ridToRouters.constBegin(); it != ridToRouters.constEnd(); ++it) {
        if (it.value().size() > 1) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Error;
            issue.message  = QString("Duplicate OSPF router-id %1 on: %2")
                                 .arg(it.key(), it.value().join(", "));
            issues.append(issue);
        }
    }
}

// ---------------------------------------------------------------------------
// Warn about configured interfaces that are not connected to any link
// ---------------------------------------------------------------------------
void Validator::checkUnconnectedInterfaces(Network *network, QList<ValidationIssue> &issues)
{
    for (const auto *dev : network->devices()) {
        for (const auto &iface : dev->interfaces()) {
            if (!iface.isConfigured()) continue;
            if (!network->interfaceInUse(dev->id(), iface.name)) {
                ValidationIssue issue;
                issue.severity = ValidationIssue::Severity::Warning;
                issue.message  = QString("'%1' interface %2 (%3) is configured but not connected.")
                                     .arg(dev->name(), iface.name, iface.ipAddress);
                issue.deviceIds << dev->id();
                issues.append(issue);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Check that RIPv2 network statements cover at least one local interface
// ---------------------------------------------------------------------------
void Validator::checkRipNetworks(Network *network, QList<ValidationIssue> &issues)
{
    for (auto *router : network->routers()) {
        if (router->routingProtocol() != Router::RoutingProtocol::RIPv2) continue;
        if (router->ripv2Config().networks.isEmpty()) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Warning;
            issue.message  = QString("RIPv2 router '%1' has no network statements configured.")
                                 .arg(router->name());
            issue.deviceIds << router->id();
            issues.append(issue);
        }
    }
}

// ---------------------------------------------------------------------------
// Basic reachability: BFS on the physical topology; warn about isolated devices
// ---------------------------------------------------------------------------
void Validator::checkReachability(Network *network, QList<ValidationIssue> &issues)
{
    const auto allDevices = network->devices();
    if (allDevices.isEmpty()) return;

    QSet<QString> visited;
    QQueue<QString> queue;
    queue.enqueue(allDevices.first()->id());

    while (!queue.isEmpty()) {
        const QString current = queue.dequeue();
        if (visited.contains(current)) continue;
        visited.insert(current);
        for (const Link *link : network->linksForDevice(current)) {
            const Device *nbr = network->neighbor(link, current);
            if (nbr && !visited.contains(nbr->id()))
                queue.enqueue(nbr->id());
        }
    }

    for (const auto *dev : allDevices) {
        if (!visited.contains(dev->id())) {
            ValidationIssue issue;
            issue.severity = ValidationIssue::Severity::Warning;
            issue.message  = QString("Device '%1' is not connected to the rest of the network.")
                                 .arg(dev->name());
            issue.deviceIds << dev->id();
            issues.append(issue);
        }
    }
}
