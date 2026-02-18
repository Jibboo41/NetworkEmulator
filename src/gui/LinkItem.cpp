#include "gui/LinkItem.h"
#include "gui/DeviceItem.h"
#include <QPainter>
#include <QPen>
#include <QLineF>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneMouseEvent>
#include <QStyleOptionGraphicsItem>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <cmath>

LinkItem::LinkItem(const Link &link, DeviceItem *source, DeviceItem *dest, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_link(link)
    , m_source(source)
    , m_dest(dest)
{
    setZValue(0);
    setFlag(ItemIsSelectable);
    source->addLink(this);
    dest->addLink(this);
    updatePosition();
}

void LinkItem::updatePosition()
{
    prepareGeometryChange();
    m_line = QLineF(m_source->pos(), m_dest->pos());
    update();
}

QRectF LinkItem::boundingRect() const
{
    // Start with the line itself plus pen-width margin
    QRectF rect = QRectF(m_line.p1(), m_line.p2()).normalized().adjusted(-2, -2, 2, 2);

    // Grow to include the interface label text rectangles.
    // Labels are drawn at: endpoint ± unit*22 + perp*8
    // Use QFontMetrics so we account for any label length and line angle.
    const qreal len = m_line.length();
    if (len > 85.0) {
        const QPointF dir  = m_line.p2() - m_line.p1();
        const QPointF unit = dir / len;
        const QPointF perp(-unit.y() * 10, unit.x() * 10);

        const QPointF lbl1 = m_line.p1() + unit * 35.0 + perp;
        const QPointF lbl2 = m_line.p2() - unit * 35.0 + perp;

        const QFontMetrics fm(QFont("Arial", 7));
        rect = rect.united(QRectF(fm.boundingRect(m_link.interface1)).translated(lbl1));
        rect = rect.united(QRectF(fm.boundingRect(m_link.interface2)).translated(lbl2));
    }

    // Small extra margin for antialiasing / sub-pixel rendering
    return rect.adjusted(-2, -2, 2, 2);
}

QPainterPath LinkItem::shape() const
{
    // Fat hit area for easier clicking
    QPainterPath path;
    QPolygonF poly;
    const QPointF p1 = m_line.p1();
    const QPointF p2 = m_line.p2();
    const QPointF d  = (p2 - p1);
    const qreal len  = std::sqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1.0) return path;
    const QPointF perp = QPointF(-d.y(), d.x()) / len * 6.0;
    poly << p1 + perp << p2 + perp << p2 - perp << p1 - perp;
    path.addPolygon(poly);
    return path;
}

void LinkItem::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem * /*option*/,
                     QWidget * /*widget*/)
{
    if (m_line.length() < 1.0) return;

    painter->setRenderHint(QPainter::Antialiasing);

    QPen pen(isSelected() ? QColor(0, 160, 255) : QColor(50, 50, 50), 2);
    painter->setPen(pen);
    painter->drawLine(m_line);

    // Interface labels near each endpoint
    painter->setPen(QColor(80, 80, 80));
    QFont f("Arial", 7);
    painter->setFont(f);

    // Place labels just outside the device icon edge (icon half-size ~27px,
    // so offset 35px clears all device shapes).  Skip on very short links
    // where the two labels would collide.
    const QPointF dir = (m_line.p2() - m_line.p1());
    const qreal len = std::sqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len > 85.0) {
        const QPointF unit = dir / len;
        const QPointF perp(-unit.y() * 10, unit.x() * 10);

        const QPointF lbl1 = m_line.p1() + unit * 35.0 + perp;
        const QPointF lbl2 = m_line.p2() - unit * 35.0 + perp;
        painter->drawText(lbl1, m_link.interface1);
        painter->drawText(lbl2, m_link.interface2);
    }
}

void LinkItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QWidget *parentWidget = nullptr;
    if (scene() && !scene()->views().isEmpty())
        parentWidget = scene()->views().first();

    QDialog dlg(parentWidget);
    dlg.setWindowTitle("Link Properties");
    auto *layout = new QFormLayout(&dlg);
    layout->addRow("Source device:",  new QLabel(m_source->device()->name()));
    layout->addRow("Source interface:", new QLabel(m_link.interface1));
    layout->addRow("Dest device:",    new QLabel(m_dest->device()->name()));
    layout->addRow("Dest interface:",  new QLabel(m_link.interface2));
    layout->addRow("Bandwidth:",       new QLabel(QString("%1 Mbps").arg(m_link.bandwidth)));
    layout->addRow("Delay:",           new QLabel(QString("%1 ms").arg(m_link.delay)));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
    layout->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    dlg.exec();
    event->accept();
}
