#include "models/Device.h"
#include <QUuid>
#include <QJsonArray>

// ---------------------------------------------------------------------------
// NetworkInterface
// ---------------------------------------------------------------------------
QJsonObject NetworkInterface::toJson() const
{
    QJsonObject obj;
    obj["name"]        = name;
    obj["ipAddress"]   = ipAddress;
    obj["subnetMask"]  = subnetMask;
    obj["ospfCost"]    = ospfCost;
    obj["description"] = description;
    return obj;
}

NetworkInterface NetworkInterface::fromJson(const QJsonObject &obj)
{
    NetworkInterface iface;
    iface.name        = obj["name"].toString();
    iface.ipAddress   = obj["ipAddress"].toString();
    iface.subnetMask  = obj["subnetMask"].toString();
    iface.ospfCost    = obj["ospfCost"].toInt(1);
    iface.description = obj["description"].toString();
    return iface;
}

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------
Device::Device(Type type, const QString &name, QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_type(type)
    , m_name(name)
{}

NetworkInterface *Device::addInterface(const QString &name)
{
    m_interfaces.append(NetworkInterface{name, {}, {}, 1, {}});
    return &m_interfaces.last();
}

NetworkInterface *Device::getInterface(const QString &name)
{
    for (auto &iface : m_interfaces)
        if (iface.name == name) return &iface;
    return nullptr;
}

const NetworkInterface *Device::getInterface(const QString &name) const
{
    for (const auto &iface : m_interfaces)
        if (iface.name == name) return &iface;
    return nullptr;
}

QJsonObject Device::toJson() const
{
    QJsonObject obj;
    obj["id"]   = m_id;
    obj["name"] = m_name;
    obj["x"]    = m_x;
    obj["y"]    = m_y;

    QJsonArray ifaceArray;
    for (const auto &iface : m_interfaces)
        ifaceArray.append(iface.toJson());
    obj["interfaces"] = ifaceArray;
    return obj;
}

void Device::populateInterfacesFromJson(const QJsonObject &obj)
{
    m_id   = obj["id"].toString();
    m_name = obj["name"].toString();
    m_x    = obj["x"].toDouble();
    m_y    = obj["y"].toDouble();

    m_interfaces.clear();
    for (const auto &v : obj["interfaces"].toArray())
        m_interfaces.append(NetworkInterface::fromJson(v.toObject()));
}

// ---------------------------------------------------------------------------
// Router
// ---------------------------------------------------------------------------
Router::Router(const QString &name, QObject *parent)
    : Device(Type::Router, name, parent)
{
    for (int i = 0; i < 4; ++i)
        addInterface(QString("Gi0/%1").arg(i));
}

QJsonObject Router::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["type"] = "Router";

    switch (m_protocol) {
        case RoutingProtocol::Static: obj["protocol"] = "Static"; break;
        case RoutingProtocol::RIPv2:  obj["protocol"] = "RIPv2";  break;
        case RoutingProtocol::OSPF:   obj["protocol"] = "OSPF";   break;
        case RoutingProtocol::PIM_DM: obj["protocol"] = "PIM-DM"; break;
    }

    QJsonArray routes;
    for (const auto &r : m_staticRoutes) {
        QJsonObject ro;
        ro["destination"] = r.destination;
        ro["mask"]        = r.mask;
        ro["nextHop"]     = r.nextHop;
        ro["metric"]      = r.metric;
        routes.append(ro);
    }
    obj["staticRoutes"] = routes;

    QJsonObject ospf;
    ospf["routerId"]   = m_ospfConfig.routerId;
    ospf["area"]       = m_ospfConfig.area;
    ospf["processId"]  = m_ospfConfig.processId;
    obj["ospfConfig"]  = ospf;

    QJsonArray ripNets;
    for (const auto &n : m_ripv2Config.networks) ripNets.append(n);
    obj["ripv2Networks"] = ripNets;

    QJsonArray pimIfaces;
    for (const auto &i : m_pimdmConfig.enabledInterfaces) pimIfaces.append(i);
    obj["pimDmInterfaces"] = pimIfaces;

    return obj;
}

Router *Router::fromJson(const QJsonObject &obj, QObject *parent)
{
    auto *r = new Router(QString(), parent);
    r->populateInterfacesFromJson(obj);

    const QString proto = obj["protocol"].toString("Static");
    if (proto == "RIPv2")  r->m_protocol = RoutingProtocol::RIPv2;
    else if (proto == "OSPF")   r->m_protocol = RoutingProtocol::OSPF;
    else if (proto == "PIM-DM") r->m_protocol = RoutingProtocol::PIM_DM;
    else                        r->m_protocol = RoutingProtocol::Static;

    for (const auto &v : obj["staticRoutes"].toArray()) {
        const QJsonObject ro = v.toObject();
        StaticRoute sr;
        sr.destination = ro["destination"].toString();
        sr.mask        = ro["mask"].toString();
        sr.nextHop     = ro["nextHop"].toString();
        sr.metric      = ro["metric"].toInt(1);
        r->m_staticRoutes.append(sr);
    }

    const QJsonObject ospf = obj["ospfConfig"].toObject();
    r->m_ospfConfig.routerId  = ospf["routerId"].toString();
    r->m_ospfConfig.area      = ospf["area"].toString("0");
    r->m_ospfConfig.processId = ospf["processId"].toInt(1);

    for (const auto &v : obj["ripv2Networks"].toArray())
        r->m_ripv2Config.networks.append(v.toString());

    for (const auto &v : obj["pimDmInterfaces"].toArray())
        r->m_pimdmConfig.enabledInterfaces.append(v.toString());

    return r;
}

// ---------------------------------------------------------------------------
// Switch
// ---------------------------------------------------------------------------
Switch::Switch(const QString &name, QObject *parent)
    : Device(Type::Switch, name, parent)
{
    for (int i = 0; i < 8; ++i)
        addInterface(QString("Fa0/%1").arg(i));
}

QJsonObject Switch::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["type"] = "Switch";
    return obj;
}

Switch *Switch::fromJson(const QJsonObject &obj, QObject *parent)
{
    auto *s = new Switch(QString(), parent);
    s->populateInterfacesFromJson(obj);
    return s;
}

// ---------------------------------------------------------------------------
// Hub
// ---------------------------------------------------------------------------
Hub::Hub(const QString &name, QObject *parent)
    : Device(Type::Hub, name, parent)
{
    for (int i = 0; i < 4; ++i)
        addInterface(QString("Port%1").arg(i));
}

QJsonObject Hub::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["type"] = "Hub";
    return obj;
}

Hub *Hub::fromJson(const QJsonObject &obj, QObject *parent)
{
    auto *h = new Hub(QString(), parent);
    h->populateInterfacesFromJson(obj);
    return h;
}

// ---------------------------------------------------------------------------
// PC
// ---------------------------------------------------------------------------
PC::PC(const QString &name, QObject *parent)
    : Device(Type::PC, name, parent)
{
    addInterface("eth0");
}

QString PC::ipAddress() const
{
    return interfaces().isEmpty() ? QString() : interfaces().first().ipAddress;
}

QString PC::subnetMask() const
{
    return interfaces().isEmpty() ? QString() : interfaces().first().subnetMask;
}

QJsonObject PC::toJson() const
{
    QJsonObject obj = Device::toJson();
    obj["type"]           = "PC";
    obj["defaultGateway"] = m_defaultGateway;
    return obj;
}

PC *PC::fromJson(const QJsonObject &obj, QObject *parent)
{
    auto *pc = new PC(QString(), parent);
    pc->populateInterfacesFromJson(obj);
    pc->m_defaultGateway = obj["defaultGateway"].toString();
    return pc;
}
