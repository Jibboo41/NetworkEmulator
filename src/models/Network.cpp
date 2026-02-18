#include "models/Network.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>

Network::Network(QObject *parent) : QObject(parent) {}

// ---------------------------------------------------------------------------
// Devices
// ---------------------------------------------------------------------------
void Network::addDevice(Device *device)
{
    device->setParent(this);
    m_devices.insert(device->id(), device);
    emit modified();
}

void Network::removeDevice(const QString &deviceId)
{
    // Remove all links referencing this device
    QStringList toRemove;
    for (auto it = m_links.begin(); it != m_links.end(); ++it)
        if (it->device1Id == deviceId || it->device2Id == deviceId)
            toRemove.append(it->id);
    for (const auto &id : toRemove) m_links.remove(id);

    if (Device *d = m_devices.take(deviceId)) delete d;
    emit modified();
}

Device *Network::device(const QString &id) const { return m_devices.value(id, nullptr); }

QList<Device *> Network::devices() const { return m_devices.values(); }

QList<Router *> Network::routers() const
{
    QList<Router *> result;
    for (auto *d : m_devices)
        if (auto *r = qobject_cast<Router *>(d)) result.append(r);
    return result;
}

QList<PC *> Network::pcs() const
{
    QList<PC *> result;
    for (auto *d : m_devices)
        if (auto *p = qobject_cast<PC *>(d)) result.append(p);
    return result;
}

// ---------------------------------------------------------------------------
// Links
// ---------------------------------------------------------------------------
void Network::addLink(const Link &link)
{
    m_links.insert(link.id, link);
    emit modified();
}

void Network::removeLink(const QString &linkId)
{
    m_links.remove(linkId);
    emit modified();
}

const Link *Network::link(const QString &id) const
{
    auto it = m_links.constFind(id);
    return (it != m_links.constEnd()) ? &it.value() : nullptr;
}

QList<const Link *> Network::links() const
{
    QList<const Link *> result;
    for (const auto &l : m_links) result.append(&l);
    return result;
}

QList<const Link *> Network::linksForDevice(const QString &deviceId) const
{
    QList<const Link *> result;
    for (const auto &l : m_links)
        if (l.device1Id == deviceId || l.device2Id == deviceId) result.append(&l);
    return result;
}

// ---------------------------------------------------------------------------
// Topology helpers
// ---------------------------------------------------------------------------
Device *Network::neighbor(const Link *link, const QString &deviceId) const
{
    if (!link) return nullptr;
    if (link->device1Id == deviceId) return device(link->device2Id);
    if (link->device2Id == deviceId) return device(link->device1Id);
    return nullptr;
}

QString Network::interfaceForLink(const Link *link, const QString &deviceId) const
{
    if (!link) return {};
    if (link->device1Id == deviceId) return link->interface1;
    if (link->device2Id == deviceId) return link->interface2;
    return {};
}

QString Network::availableInterface(const QString &deviceId) const
{
    Device *d = device(deviceId);
    if (!d) return {};

    QSet<QString> used;
    for (const auto &l : m_links) {
        if (l.device1Id == deviceId) used.insert(l.interface1);
        if (l.device2Id == deviceId) used.insert(l.interface2);
    }

    for (const auto &iface : d->interfaces())
        if (!used.contains(iface.name)) return iface.name;
    return {};
}

bool Network::interfaceInUse(const QString &deviceId, const QString &ifaceName) const
{
    for (const auto &l : m_links)
        if ((l.device1Id == deviceId && l.interface1 == ifaceName) ||
            (l.device2Id == deviceId && l.interface2 == ifaceName))
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------
bool Network::save(const QString &filePath, QString *error) const
{
    QJsonObject root;
    root["name"] = m_name;

    QJsonArray devArray;
    for (const auto *d : m_devices) devArray.append(d->toJson());
    root["devices"] = devArray;

    QJsonArray linkArray;
    for (const auto &l : m_links) linkArray.append(l.toJson());
    root["links"] = linkArray;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    return true;
}

bool Network::load(const QString &filePath, QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) *error = parseError.errorString();
        return false;
    }

    clear();

    const QJsonObject root = doc.object();
    m_name = root["name"].toString("Untitled Network");

    for (const auto &v : root["devices"].toArray()) {
        const QJsonObject dObj = v.toObject();
        const QString type = dObj["type"].toString();
        Device *d = nullptr;
        if      (type == "Router") d = Router::fromJson(dObj, this);
        else if (type == "Switch") d = Switch::fromJson(dObj, this);
        else if (type == "Hub")    d = Hub::fromJson(dObj, this);
        else if (type == "PC")     d = PC::fromJson(dObj, this);
        if (d) m_devices.insert(d->id(), d);
    }

    for (const auto &v : root["links"].toArray()) {
        const Link l = Link::fromJson(v.toObject());
        m_links.insert(l.id, l);
    }

    emit modified();
    return true;
}

void Network::clear()
{
    qDeleteAll(m_devices);
    m_devices.clear();
    m_links.clear();
    m_name = "Untitled Network";
    emit modified();
}
