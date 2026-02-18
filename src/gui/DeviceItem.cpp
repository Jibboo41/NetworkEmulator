#include "gui/DeviceItem.h"
#include "gui/LinkItem.h"
#include "gui/dialogs/RouterDialog.h"
#include "gui/dialogs/SwitchDialog.h"
#include "gui/dialogs/PCDialog.h"
#include "gui/dialogs/HubDialog.h"
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPolygonF>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QStyleOptionGraphicsItem>

static QColor deviceColor(Device::Type t)
{
    switch (t) {
        case Device::Type::Router: return QColor(52, 120, 200);   // blue
        case Device::Type::Switch: return QColor(48, 172, 110);   // green
        case Device::Type::Hub:    return QColor(230, 140, 30);   // orange
        case Device::Type::PC:     return QColor(120, 120, 130);  // slate
    }
    return Qt::gray;
}

DeviceItem::DeviceItem(Device *device, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_device(device)
{
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setZValue(1);
    setToolTip(device->name());
}

void DeviceItem::addLink(LinkItem *link)    { m_links.append(link); }
void DeviceItem::removeLink(LinkItem *link) { m_links.removeAll(link); }

QRectF DeviceItem::boundingRect() const
{
    const qreal half = SIZE / 2.0;
    // Extra 20px below for the name label
    return QRectF(-half - 2, -half - 2, SIZE + 4, SIZE + 24);
}

void DeviceItem::paint(QPainter *painter,
                       const QStyleOptionGraphicsItem * /*option*/,
                       QWidget * /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing);
    const qreal half = SIZE / 2.0;
    const QColor body = deviceColor(m_device->deviceType());

    // Border
    QPen borderPen;
    if (m_highlighted)
        borderPen = QPen(QColor(255, 220, 0), 3);
    else if (isSelected())
        borderPen = QPen(QColor(0, 160, 255), 3);
    else
        borderPen = QPen(QColor(30, 30, 30), 1.5);

    painter->setPen(borderPen);
    painter->setBrush(QBrush(body));

    switch (m_device->deviceType()) {
        case Device::Type::Router:
            painter->drawEllipse(QRectF(-half, -half, SIZE, SIZE));
            break;
        case Device::Type::Switch:
            painter->drawRoundedRect(QRectF(-half, -half, SIZE, SIZE), 8, 8);
            break;
        case Device::Type::Hub: {
            // Diamond shape
            QPolygonF diamond;
            diamond << QPointF(0, -half) << QPointF(half, 0)
                    << QPointF(0,  half) << QPointF(-half, 0);
            painter->drawPolygon(diamond);
            break;
        }
        case Device::Type::PC:
            painter->drawRect(QRectF(-half, -half, SIZE, SIZE));
            break;
    }

    // Icon label inside the shape
    painter->setPen(Qt::white);
    QFont iconFont("Arial", 13, QFont::Bold);
    painter->setFont(iconFont);
    const QRectF iconRect(-half, -half, SIZE, SIZE);

    QString label;
    switch (m_device->deviceType()) {
        case Device::Type::Router: label = "R";   break;
        case Device::Type::Switch: label = "SW";  break;
        case Device::Type::Hub:    label = "HUB"; break;
        case Device::Type::PC:     label = "PC";  break;
    }
    painter->drawText(iconRect, Qt::AlignCenter, label);

    // Device name below
    painter->setPen(Qt::black);
    QFont nameFont("Arial", 8);
    painter->setFont(nameFont);
    painter->drawText(QRectF(-40, half + 3, 80, 18), Qt::AlignCenter, m_device->name());
}

QVariant DeviceItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        const QPointF pos = value.toPointF();
        m_device->setPosition(pos.x(), pos.y());
        for (LinkItem *link : m_links)
            link->updatePosition();
    }
    return QGraphicsObject::itemChange(change, value);
}

void DeviceItem::openConfigDialog()
{
    QWidget *parentWidget = nullptr;
    if (scene() && !scene()->views().isEmpty())
        parentWidget = scene()->views().first();

    switch (m_device->deviceType()) {
        case Device::Type::Router: {
            RouterDialog dlg(qobject_cast<Router *>(m_device), parentWidget);
            dlg.exec();
            break;
        }
        case Device::Type::Switch: {
            SwitchDialog dlg(qobject_cast<Switch *>(m_device), parentWidget);
            dlg.exec();
            break;
        }
        case Device::Type::Hub: {
            HubDialog dlg(qobject_cast<Hub *>(m_device), parentWidget);
            dlg.exec();
            break;
        }
        case Device::Type::PC: {
            PCDialog dlg(qobject_cast<PC *>(m_device), parentWidget);
            dlg.exec();
            break;
        }
    }
    update();
    setToolTip(m_device->name());
}

void DeviceItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    openConfigDialog();
    if (event) event->accept();
}

void DeviceItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu menu;
    QAction *configAction = menu.addAction("Configure...");
    QAction *deleteAction = menu.addAction("Delete");

    QAction *chosen = menu.exec(event->screenPos());
    event->accept();

    if (chosen == configAction) {
        openConfigDialog();
    } else if (chosen == deleteAction) {
        // Emit via queued connection so this event handler fully unwinds
        // before the item is deleted by NetworkCanvas.
        emit deleteRequested();
    }
}
