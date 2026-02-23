#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include "utils/IpUtils.h"

// ---------------------------------------------------------------------------
// NetworkInterface
// ---------------------------------------------------------------------------
struct NetworkInterface {
    QString name;
    QString ipAddress;
    QString subnetMask;
    int     ospfCost   = 1;
    QString description;
    // When the owning Router is designated as Host PC, this holds the
    // name of the real host network adapter this virtual interface maps to
    // (e.g. "eth0", "enp3s0", "wlan0"). Empty = no mapping.
    QString hostInterfaceName;

    bool    isConfigured() const { return !ipAddress.isEmpty() && !subnetMask.isEmpty(); }
    quint32 ipAsUint32()   const { return IpUtils::parse(ipAddress); }
    quint32 maskAsUint32() const { return IpUtils::parse(subnetMask); }
    quint32 networkAddr()  const { return IpUtils::networkAddress(ipAsUint32(), maskAsUint32()); }
    int     prefixLen()    const { return IpUtils::maskToPrefix(maskAsUint32()); }

    QJsonObject toJson() const;
    static NetworkInterface fromJson(const QJsonObject &obj);
};

// ---------------------------------------------------------------------------
// RoutingEntry  (populated by simulation)
// ---------------------------------------------------------------------------
struct RoutingEntry {
    QString destination;
    QString mask;
    QString nextHop;       // IP string or "directly connected"
    QString exitInterface;
    int     metric   = 0;
    QString protocol; // "Connected", "Static", "RIPv2", "OSPF", "PIM-DM"
};

// ---------------------------------------------------------------------------
// Device  (base class)
// ---------------------------------------------------------------------------
class Device : public QObject
{
    Q_OBJECT
public:
    enum class Type { Router, Switch, Hub, PC };

    Device(Type type, const QString &name = QString(), QObject *parent = nullptr);

    QString id()         const { return m_id; }
    Type    deviceType() const { return m_type; }
    QString name()       const { return m_name; }
    void    setName(const QString &n) { m_name = n; }

    qreal x() const { return m_x; }
    qreal y() const { return m_y; }
    void  setPosition(qreal x, qreal y) { m_x = x; m_y = y; }

    QList<NetworkInterface>       &interfaces()       { return m_interfaces; }
    const QList<NetworkInterface> &interfaces() const { return m_interfaces; }

    NetworkInterface *addInterface(const QString &name);
    NetworkInterface *getInterface(const QString &name);
    const NetworkInterface *getInterface(const QString &name) const;

    virtual QJsonObject toJson() const;

protected:
    void populateInterfacesFromJson(const QJsonObject &obj);

private:
    QString m_id;
    Type    m_type;
    QString m_name;
    qreal   m_x = 0.0;
    qreal   m_y = 0.0;
    QList<NetworkInterface> m_interfaces;
};

// ---------------------------------------------------------------------------
// Router
// ---------------------------------------------------------------------------
class Router : public Device
{
    Q_OBJECT
public:
    enum class RoutingProtocol { Static, RIPv2, OSPF, PIM_DM };

    struct StaticRoute {
        QString destination;
        QString mask;
        QString nextHop;
        int     metric = 1;
    };

    struct OSPFConfig {
        QString routerId;
        QString area      = "0";
        int     processId = 1;
    };

    struct RIPv2Config {
        QStringList networks; // Networks to advertise
    };

    struct PIMDMConfig {
        QStringList enabledInterfaces;
    };

    explicit Router(const QString &name = QString(), QObject *parent = nullptr);

    RoutingProtocol routingProtocol() const      { return m_protocol; }
    void setRoutingProtocol(RoutingProtocol p)   { m_protocol = p; }

    QList<StaticRoute> &staticRoutes()           { return m_staticRoutes; }
    OSPFConfig         &ospfConfig()             { return m_ospfConfig; }
    RIPv2Config        &ripv2Config()            { return m_ripv2Config; }
    PIMDMConfig        &pimdmConfig()            { return m_pimdmConfig; }

    const QList<StaticRoute> &staticRoutes() const { return m_staticRoutes; }
    const OSPFConfig         &ospfConfig()   const { return m_ospfConfig; }
    const RIPv2Config        &ripv2Config()  const { return m_ripv2Config; }
    const PIMDMConfig        &pimdmConfig()  const { return m_pimdmConfig; }

    // Host PC designation ---------------------------------------------------
    // When true, this router represents the physical machine running the emulator.
    // Each interface's hostInterfaceName maps to a real host network adapter.
    bool isHostPC() const          { return m_isHostPC; }
    void setIsHostPC(bool v)       { m_isHostPC = v; }

    // Computed by simulation ------------------------------------------------
    QList<RoutingEntry>       &computedRoutingTable()       { return m_routingTable; }
    const QList<RoutingEntry> &computedRoutingTable() const { return m_routingTable; }
    void clearRoutingTable()                         { m_routingTable.clear(); }
    void addRoutingEntry(const RoutingEntry &entry)  { m_routingTable.append(entry); }

    QJsonObject toJson() const override;
    static Router *fromJson(const QJsonObject &obj, QObject *parent = nullptr);

private:
    RoutingProtocol    m_protocol = RoutingProtocol::Static;
    bool               m_isHostPC = false;
    QList<StaticRoute> m_staticRoutes;
    OSPFConfig         m_ospfConfig;
    RIPv2Config        m_ripv2Config;
    PIMDMConfig        m_pimdmConfig;
    QList<RoutingEntry> m_routingTable;
};

// ---------------------------------------------------------------------------
// Switch
// ---------------------------------------------------------------------------
class Switch : public Device
{
    Q_OBJECT
public:
    explicit Switch(const QString &name = QString(), QObject *parent = nullptr);
    QJsonObject toJson() const override;
    static Switch *fromJson(const QJsonObject &obj, QObject *parent = nullptr);
};

// ---------------------------------------------------------------------------
// Hub
// ---------------------------------------------------------------------------
class Hub : public Device
{
    Q_OBJECT
public:
    explicit Hub(const QString &name = QString(), QObject *parent = nullptr);
    QJsonObject toJson() const override;
    static Hub *fromJson(const QJsonObject &obj, QObject *parent = nullptr);
};

// ---------------------------------------------------------------------------
// PC
// ---------------------------------------------------------------------------
class PC : public Device
{
    Q_OBJECT
public:
    explicit PC(const QString &name = QString(), QObject *parent = nullptr);

    QString defaultGateway() const          { return m_defaultGateway; }
    void setDefaultGateway(const QString &g){ m_defaultGateway = g; }

    QString ipAddress()  const;
    QString subnetMask() const;

    QJsonObject toJson() const override;
    static PC *fromJson(const QJsonObject &obj, QObject *parent = nullptr);

private:
    QString m_defaultGateway;
};
