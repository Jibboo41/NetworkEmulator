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
#include <QRadialGradient>
#include <QLinearGradient>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QStyleOptionGraphicsItem>

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

    // Selection / highlight ring drawn behind the icon
    if (isSelected() || m_highlighted) {
        const QColor ringColor = m_highlighted ? QColor(255, 215, 0) : QColor(30, 160, 255);
        QPen ringPen(ringColor, 2.5, Qt::DashLine);
        painter->setPen(ringPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(-half - 4, -half - 4, SIZE + 8, SIZE + 8), 6, 6);
    }

    switch (m_device->deviceType()) {
        case Device::Type::Router: drawRouterIcon(painter); break;
        case Device::Type::Switch: drawSwitchIcon(painter); break;
        case Device::Type::Hub:    drawHubIcon(painter);    break;
        case Device::Type::PC:     drawPCIcon(painter);     break;
    }

    // Device name label below icon
    painter->setPen(Qt::black);
    QFont nameFont("Arial", 8);
    painter->setFont(nameFont);
    painter->drawText(QRectF(-40, half + 3, 80, 18), Qt::AlignCenter, m_device->name());
}

// ---------------------------------------------------------------------------
// Router — Cisco-style circle with 4 directional arrows
// ---------------------------------------------------------------------------
void DeviceItem::drawRouterIcon(QPainter *p)
{
    const qreal R = 22.0;

    // Drop shadow
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 50));
    p->drawEllipse(QRectF(-R + 2, -R + 2, R * 2, R * 2));

    // Body with radial gradient (light top-left → deep blue)
    QRadialGradient grad(QPointF(-7, -7), R * 1.6);
    grad.setColorAt(0.0, QColor(130, 185, 255));
    grad.setColorAt(1.0, QColor(22, 80, 195));
    p->setPen(QPen(QColor(12, 55, 155), 1.5));
    p->setBrush(grad);
    p->drawEllipse(QRectF(-R, -R, R * 2, R * 2));

    // Cross hairlines (routing path symbol)
    p->setPen(QPen(QColor(255, 255, 255, 100), 1.0));
    p->drawLine(QPointF(-13, 0), QPointF(13, 0));
    p->drawLine(QPointF(0, -13), QPointF(0, 13));

    // 4 outward-pointing arrow heads at N, E, S, W
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::white);
    const qreal tip = 21.0, base = 13.0, hw = 4.5;
    QPolygonF N, E, S, W;
    N << QPointF(0, -tip)  << QPointF(-hw, -base) << QPointF(hw, -base);
    E << QPointF(tip, 0)   << QPointF(base, -hw)  << QPointF(base,  hw);
    S << QPointF(0,  tip)  << QPointF(-hw,  base)  << QPointF(hw,  base);
    W << QPointF(-tip, 0)  << QPointF(-base, -hw)  << QPointF(-base, hw);
    p->drawPolygon(N);
    p->drawPolygon(E);
    p->drawPolygon(S);
    p->drawPolygon(W);
}

// ---------------------------------------------------------------------------
// Switch — flat rack-mount box with port sockets and activity LEDs
// ---------------------------------------------------------------------------
void DeviceItem::drawSwitchIcon(QPainter *p)
{
    const qreal BW = 24.0, BH = 14.0;

    // Drop shadow
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 50));
    p->drawRoundedRect(QRectF(-BW + 2, -BH + 2, BW * 2, BH * 2), 3, 3);

    // Body (top-lit green gradient)
    QLinearGradient grad(0, -BH, 0, BH);
    grad.setColorAt(0.0, QColor(85, 215, 145));
    grad.setColorAt(1.0, QColor(18, 130, 70));
    p->setPen(QPen(QColor(8, 85, 40), 1.5));
    p->setBrush(grad);
    p->drawRoundedRect(QRectF(-BW, -BH, BW * 2, BH * 2), 3, 3);

    // Top edge sheen
    p->setPen(QPen(QColor(200, 255, 220, 80), 1.0));
    p->drawLine(QPointF(-BW + 3, -BH + 1), QPointF(BW - 3, -BH + 1));

    // 8 port sockets (3.5px wide, 5px pitch, starting at x = -19.5)
    const qreal portStart = -19.5;
    for (int i = 0; i < 8; ++i) {
        const qreal px = portStart + i * 5.0;

        // Port socket (dark inset rectangle)
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(5, 45, 18));
        p->drawRoundedRect(QRectF(px, -3, 3.5, 7), 1, 1);

        // Activity LED above port (two ports intentionally "dark" for realism)
        const bool active = (i != 2 && i != 6);
        p->setBrush(active ? QColor(80, 255, 110) : QColor(15, 65, 28));
        p->drawRect(QRectF(px + 0.5, -7, 2.5, 2));
    }
}

// ---------------------------------------------------------------------------
// Hub — rounded box with concentric broadcast arcs
// ---------------------------------------------------------------------------
void DeviceItem::drawHubIcon(QPainter *p)
{
    const qreal BW = 22.0, BH = 13.0;

    // Drop shadow
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 50));
    p->drawRoundedRect(QRectF(-BW + 2, -BH + 2, BW * 2, BH * 2), 4, 4);

    // Body (amber gradient)
    QLinearGradient grad(0, -BH, 0, BH);
    grad.setColorAt(0.0, QColor(255, 190, 70));
    grad.setColorAt(1.0, QColor(205, 110, 15));
    p->setPen(QPen(QColor(145, 70, 0), 1.5));
    p->setBrush(grad);
    p->drawRoundedRect(QRectF(-BW, -BH, BW * 2, BH * 2), 4, 4);

    // Top edge sheen
    p->setPen(QPen(QColor(255, 240, 180, 100), 1.0));
    p->drawLine(QPointF(-BW + 4, -BH + 1), QPointF(BW - 4, -BH + 1));

    // 3 concentric broadcast arcs opening upward from anchor point
    // drawArc angles: 0°=right, positive=CCW; span 30°–150° passes through 90° (top)
    const QPointF center(0.0, 3.0);
    p->setBrush(Qt::NoBrush);
    for (int i = 1; i <= 3; ++i) {
        const qreal r = i * 4.0;          // r = 4, 8, 12
        const int   alpha = 220 - (i - 1) * 55; // 220, 165, 110
        p->setPen(QPen(QColor(255, 255, 255, alpha), 1.8, Qt::SolidLine, Qt::RoundCap));
        p->drawArc(QRectF(center.x() - r, center.y() - r, r * 2, r * 2),
                   30 * 16, 120 * 16);
    }

    // Broadcast source dot
    p->setPen(Qt::NoPen);
    p->setBrush(Qt::white);
    p->drawEllipse(center, 2.5, 2.5);
}

// ---------------------------------------------------------------------------
// PC — flat-panel monitor with stand and base
// ---------------------------------------------------------------------------
void DeviceItem::drawPCIcon(QPainter *p)
{
    // Drop shadow for monitor
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 50));
    p->drawRoundedRect(QRectF(-20 + 2, -24 + 2, 40, 24), 3, 3);

    // Monitor bezel (silver-grey gradient)
    QLinearGradient bezelGrad(0, -24, 0, 0);
    bezelGrad.setColorAt(0.0, QColor(175, 175, 192));
    bezelGrad.setColorAt(1.0, QColor(88, 88, 108));
    p->setPen(QPen(QColor(45, 45, 62), 1.5));
    p->setBrush(bezelGrad);
    p->drawRoundedRect(QRectF(-20, -24, 40, 24), 3, 3);

    // Screen (blue-tinted display)
    QLinearGradient screenGrad(0, -21, 0, -3);
    screenGrad.setColorAt(0.0, QColor(42, 100, 185));
    screenGrad.setColorAt(0.6, QColor(20, 58, 138));
    screenGrad.setColorAt(1.0, QColor(10, 28, 78));
    p->setPen(Qt::NoPen);
    p->setBrush(screenGrad);
    p->drawRect(QRectF(-17, -21, 34, 18));

    // Screen glare (translucent highlight in top-left corner)
    p->setBrush(QColor(255, 255, 255, 50));
    p->drawRect(QRectF(-16, -20, 14, 5));

    // Power LED (small green dot on lower bezel)
    p->setBrush(QColor(55, 250, 85));
    p->drawEllipse(QRectF(13, -5.5, 3, 3));

    // Stand neck
    p->setPen(QPen(QColor(45, 45, 62), 1.0));
    p->setBrush(QColor(105, 105, 122));
    p->drawRect(QRectF(-4, 0, 8, 8));

    // Stand base
    p->setBrush(QColor(78, 78, 98));
    p->drawRoundedRect(QRectF(-16, 8, 32, 5), 2, 2);
}

// ---------------------------------------------------------------------------
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
