#pragma once
#include <QGraphicsView>
#include <QHash>
#include "models/Device.h"

class Network;
class DeviceItem;
class LinkItem;
class QGraphicsScene;

class NetworkCanvas : public QGraphicsView
{
    Q_OBJECT
public:
    enum class Mode { Select, PlaceRouter, PlaceSwitch, PlaceHub, PlacePC, Connect, Delete };

    explicit NetworkCanvas(Network *network, QWidget *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void rebuildFromNetwork();
    void clear();

signals:
    void statusMessage(const QString &msg);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void drawBackground(QPainter *painter, const QRectF &rect) override;

private:
    void connectItemSignals(DeviceItem *item);
    void onHostPCDesignated(DeviceItem *item, bool enable);
    void placeDevice(Device::Type type, const QPointF &scenePos);
    void startConnect(DeviceItem *item);
    void finishConnect(DeviceItem *item);
    void deleteDeviceItem(DeviceItem *item);
    void deleteLinkItem(LinkItem *item);
    DeviceItem *deviceItemAt(const QPointF &scenePos) const;
    LinkItem   *linkItemAt(const QPointF &scenePos) const;

    Network        *m_network;
    QGraphicsScene *m_scene;
    Mode            m_mode         = Mode::Select;
    DeviceItem     *m_connectSource = nullptr;

    QHash<QString, DeviceItem *> m_deviceItems; // device id  -> item
    QHash<QString, LinkItem *>   m_linkItems;   // link id    -> item
    int                          m_counter = 0;
};
