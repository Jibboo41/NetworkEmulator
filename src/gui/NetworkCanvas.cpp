#include "gui/NetworkCanvas.h"
#include "gui/DeviceItem.h"
#include "gui/LinkItem.h"
#include "models/Network.h"
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QUuid>
#include <QTransform>
#include <QMessageBox>

NetworkCanvas::NetworkCanvas(Network *network, QWidget *parent)
    : QGraphicsView(parent)
    , m_network(network)
{
    m_scene = new QGraphicsScene(this);
    m_scene->setSceneRect(-2000, -2000, 4000, 4000);
    setScene(m_scene);

    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorUnderMouse);
    setBackgroundBrush(QBrush(QColor(245, 245, 250)));
}

// ---------------------------------------------------------------------------
void NetworkCanvas::setMode(Mode mode)
{
    m_mode = mode;

    if (m_connectSource) {
        m_connectSource->setHighlighted(false);
        m_connectSource = nullptr;
    }

    switch (mode) {
        case Mode::Select:
            setDragMode(QGraphicsView::ScrollHandDrag);
            setCursor(Qt::ArrowCursor);
            emit statusMessage("Select mode: click to select, drag to move, double-click to configure.");
            break;
        case Mode::PlaceRouter:
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::CrossCursor);
            emit statusMessage("Click on the canvas to place a Router.");
            break;
        case Mode::PlaceSwitch:
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::CrossCursor);
            emit statusMessage("Click on the canvas to place a Switch.");
            break;
        case Mode::PlaceHub:
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::CrossCursor);
            emit statusMessage("Click on the canvas to place a Hub.");
            break;
        case Mode::PlacePC:
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::CrossCursor);
            emit statusMessage("Click on the canvas to place a PC.");
            break;
        case Mode::Connect:
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::PointingHandCursor);
            emit statusMessage("Connect mode: click source device, then destination device.");
            break;
        case Mode::Delete:
            setDragMode(QGraphicsView::NoDrag);
            setCursor(Qt::ForbiddenCursor);
            emit statusMessage("Delete mode: click a device or link to delete it.");
            break;
    }
}

// ---------------------------------------------------------------------------
void NetworkCanvas::clear()
{
    m_scene->clear();
    m_deviceItems.clear();
    m_linkItems.clear();
    m_connectSource = nullptr;
}

void NetworkCanvas::rebuildFromNetwork()
{
    clear();
    for (Device *dev : m_network->devices()) {
        auto *item = new DeviceItem(dev);
        item->setPos(dev->x(), dev->y());
        m_scene->addItem(item);
        m_deviceItems.insert(dev->id(), item);
        connectItemSignals(item);
    }
    for (const Link *link : m_network->links()) {
        DeviceItem *src = m_deviceItems.value(link->device1Id);
        DeviceItem *dst = m_deviceItems.value(link->device2Id);
        if (!src || !dst) continue;
        auto *litem = new LinkItem(*link, src, dst);
        m_scene->addItem(litem);
        m_linkItems.insert(link->id, litem);
    }
}

// ---------------------------------------------------------------------------
void NetworkCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    const QPointF scenePos = mapToScene(event->pos());

    switch (m_mode) {
        case Mode::PlaceRouter: placeDevice(Device::Type::Router, scenePos); break;
        case Mode::PlaceSwitch: placeDevice(Device::Type::Switch, scenePos); break;
        case Mode::PlaceHub:    placeDevice(Device::Type::Hub,    scenePos); break;
        case Mode::PlacePC:     placeDevice(Device::Type::PC,     scenePos); break;

        case Mode::Connect: {
            DeviceItem *hit = deviceItemAt(scenePos);
            if (!hit) break;
            if (!m_connectSource) {
                startConnect(hit);
            } else if (hit != m_connectSource) {
                finishConnect(hit);
            }
            break;
        }

        case Mode::Delete: {
            DeviceItem *dItem = deviceItemAt(scenePos);
            if (dItem) { deleteDeviceItem(dItem); break; }
            LinkItem *lItem = linkItemAt(scenePos);
            if (lItem) { deleteLinkItem(lItem); break; }
            break;
        }

        case Mode::Select:
            QGraphicsView::mousePressEvent(event);
            break;
    }
}

void NetworkCanvas::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        const auto selected = m_scene->selectedItems();
        for (auto *item : selected) {
            if (auto *di = qgraphicsitem_cast<DeviceItem *>(item)) {
                deleteDeviceItem(di);
            } else if (auto *li = qgraphicsitem_cast<LinkItem *>(item)) {
                deleteLinkItem(li);
            }
        }
    }
    QGraphicsView::keyPressEvent(event);
}

void NetworkCanvas::dragEnterEvent(QDragEnterEvent * /*event*/) {}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void NetworkCanvas::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawBackground(painter, rect);

    // Light grid
    const int step = 40;
    painter->setPen(QPen(QColor(210, 210, 220), 0.5));

    const int left  = static_cast<int>(rect.left())  - (static_cast<int>(rect.left())  % step);
    const int top   = static_cast<int>(rect.top())   - (static_cast<int>(rect.top())   % step);

    for (int x = left; x < rect.right();  x += step)
        painter->drawLine(x, static_cast<int>(rect.top()), x, static_cast<int>(rect.bottom()));
    for (int y = top;  y < rect.bottom(); y += step)
        painter->drawLine(static_cast<int>(rect.left()), y, static_cast<int>(rect.right()), y);
}

// ---------------------------------------------------------------------------
// Signal wiring
// ---------------------------------------------------------------------------
void NetworkCanvas::connectItemSignals(DeviceItem *item)
{
    // Use Qt::QueuedConnection so the item's own event handler fully unwinds
    // before deleteDeviceItem() is called and the item is destroyed.
    connect(item, &DeviceItem::deleteRequested, this, [this, item]() {
        deleteDeviceItem(item);
    }, Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------
void NetworkCanvas::placeDevice(Device::Type type, const QPointF &scenePos)
{
    ++m_counter;
    QString name;
    Device *dev = nullptr;

    switch (type) {
        case Device::Type::Router:
            name = QString("R%1").arg(m_counter);
            dev  = new Router(name, m_network);
            break;
        case Device::Type::Switch:
            name = QString("SW%1").arg(m_counter);
            dev  = new Switch(name, m_network);
            break;
        case Device::Type::Hub:
            name = QString("Hub%1").arg(m_counter);
            dev  = new Hub(name, m_network);
            break;
        case Device::Type::PC:
            name = QString("PC%1").arg(m_counter);
            dev  = new PC(name, m_network);
            break;
    }

    dev->setPosition(scenePos.x(), scenePos.y());
    m_network->addDevice(dev);

    auto *item = new DeviceItem(dev);
    item->setPos(scenePos);
    m_scene->addItem(item);
    m_deviceItems.insert(dev->id(), item);
    connectItemSignals(item);

    emit statusMessage(QString("Placed %1. Double-click to configure.").arg(name));
}

// ---------------------------------------------------------------------------
// Connect
// ---------------------------------------------------------------------------
void NetworkCanvas::startConnect(DeviceItem *item)
{
    m_connectSource = item;
    item->setHighlighted(true);
    emit statusMessage(QString("Source: %1. Now click the destination device.")
                       .arg(item->device()->name()));
}

void NetworkCanvas::finishConnect(DeviceItem *item)
{
    DeviceItem *src = m_connectSource;
    DeviceItem *dst = item;
    m_connectSource->setHighlighted(false);
    m_connectSource = nullptr;

    const QString if1 = m_network->availableInterface(src->device()->id());
    const QString if2 = m_network->availableInterface(dst->device()->id());

    if (if1.isEmpty()) {
        QMessageBox::warning(this, "No Free Interface",
            QString("%1 has no free interfaces.").arg(src->device()->name()));
        return;
    }
    if (if2.isEmpty()) {
        QMessageBox::warning(this, "No Free Interface",
            QString("%1 has no free interfaces.").arg(dst->device()->name()));
        return;
    }

    Link link;
    link.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    link.device1Id = src->device()->id();
    link.interface1 = if1;
    link.device2Id = dst->device()->id();
    link.interface2 = if2;
    m_network->addLink(link);

    auto *litem = new LinkItem(link, src, dst);
    m_scene->addItem(litem);
    m_linkItems.insert(link.id, litem);

    emit statusMessage(QString("Connected %1 (%2) <-> %3 (%4)")
                       .arg(src->device()->name(), if1,
                            dst->device()->name(), if2));
}

// ---------------------------------------------------------------------------
// Delete
// ---------------------------------------------------------------------------
void NetworkCanvas::deleteDeviceItem(DeviceItem *item)
{
    const QString devId = item->device()->id();

    // Remove all link items connected to this device
    const QList<const Link *> links = m_network->linksForDevice(devId);
    for (const Link *link : links) {
        if (LinkItem *li = m_linkItems.take(link->id)) {
            li->sourceItem()->removeLink(li);
            li->destItem()->removeLink(li);
            m_scene->removeItem(li);
            delete li;
        }
    }

    m_deviceItems.remove(devId);
    m_scene->removeItem(item);
    delete item;

    m_network->removeDevice(devId); // also removes links from model
    emit statusMessage("Device deleted.");
}

void NetworkCanvas::deleteLinkItem(LinkItem *item)
{
    const QString linkId = item->link().id;
    item->sourceItem()->removeLink(item);
    item->destItem()->removeLink(item);
    m_scene->removeItem(item);
    m_linkItems.remove(linkId);
    delete item;
    m_network->removeLink(linkId);
    emit statusMessage("Link deleted.");
}

// ---------------------------------------------------------------------------
// Hit testing
// ---------------------------------------------------------------------------
DeviceItem *NetworkCanvas::deviceItemAt(const QPointF &scenePos) const
{
    for (QGraphicsItem *item : m_scene->items(scenePos, Qt::IntersectsItemBoundingRect,
                                              Qt::DescendingOrder)) {
        if (auto *di = qgraphicsitem_cast<DeviceItem *>(item)) return di;
    }
    return nullptr;
}

LinkItem *NetworkCanvas::linkItemAt(const QPointF &scenePos) const
{
    for (QGraphicsItem *item : m_scene->items(scenePos, Qt::IntersectsItemBoundingRect,
                                              Qt::DescendingOrder)) {
        if (auto *li = qgraphicsitem_cast<LinkItem *>(item)) return li;
    }
    return nullptr;
}
