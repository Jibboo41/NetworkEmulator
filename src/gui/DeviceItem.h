#pragma once
#include <QGraphicsObject>
#include <QRectF>
#include "models/Device.h"

class LinkItem;

class DeviceItem : public QGraphicsObject
{
    Q_OBJECT
public:
    static constexpr qreal SIZE = 54.0;

    explicit DeviceItem(Device *device, QGraphicsItem *parent = nullptr);

    Device *device() const { return m_device; }

    void addLink(LinkItem *link);
    void removeLink(LinkItem *link);

    void setHighlighted(bool on) { m_highlighted = on; update(); }
    bool isHighlighted() const   { return m_highlighted; }

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void   paint(QPainter *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *widget = nullptr) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void     mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void     contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    Device          *m_device;
    QList<LinkItem *> m_links;
    bool             m_highlighted = false;
};
