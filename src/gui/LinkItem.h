#pragma once
#include <QGraphicsObject>
#include "models/Link.h"

class DeviceItem;

class LinkItem : public QGraphicsObject
{
    Q_OBJECT
public:
    LinkItem(const Link &link,
             DeviceItem *source,
             DeviceItem *dest,
             QGraphicsItem *parent = nullptr);

    const Link &link() const { return m_link; }
    DeviceItem *sourceItem() const { return m_source; }
    DeviceItem *destItem()   const { return m_dest; }

    void updatePosition();

    QRectF boundingRect() const override;
    void   paint(QPainter *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *widget = nullptr) override;

protected:
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    QPainterPath shape() const override;

private:
    Link        m_link;
    DeviceItem *m_source;
    DeviceItem *m_dest;
    QLineF      m_line;
};
