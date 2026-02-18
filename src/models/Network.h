#pragma once
#include <QObject>
#include <QHash>
#include <QList>
#include "models/Device.h"
#include "models/Link.h"

class Network : public QObject
{
    Q_OBJECT
public:
    explicit Network(QObject *parent = nullptr);

    // --- Devices ---
    void    addDevice(Device *device);
    void    removeDevice(const QString &deviceId);
    Device *device(const QString &id) const;
    QList<Device *> devices() const;
    QList<Router *> routers() const;
    QList<PC *>     pcs()     const;

    // --- Links ---
    void    addLink(const Link &link);
    void    removeLink(const QString &linkId);
    const Link *link(const QString &id) const;
    QList<const Link *> links() const;
    QList<const Link *> linksForDevice(const QString &deviceId) const;

    // --- Topology helpers ---
    Device *neighbor(const Link *link, const QString &deviceId) const;
    QString interfaceForLink(const Link *link, const QString &deviceId) const;
    QString availableInterface(const QString &deviceId) const;
    bool    interfaceInUse(const QString &deviceId, const QString &ifaceName) const;

    // --- Persistence ---
    bool save(const QString &filePath, QString *error = nullptr) const;
    bool load(const QString &filePath, QString *error = nullptr);
    void clear();

    QString name() const        { return m_name; }
    void    setName(const QString &n) { m_name = n; }

signals:
    void modified();

private:
    QHash<QString, Device *> m_devices; // owns devices (parent = this)
    QHash<QString, Link>     m_links;
    QString                  m_name = "Untitled Network";
};
