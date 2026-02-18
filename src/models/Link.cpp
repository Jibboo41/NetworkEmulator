#include "models/Link.h"

QJsonObject Link::toJson() const
{
    QJsonObject obj;
    obj["id"]         = id;
    obj["device1Id"]  = device1Id;
    obj["interface1"] = interface1;
    obj["device2Id"]  = device2Id;
    obj["interface2"] = interface2;
    obj["bandwidth"]  = bandwidth;
    obj["delay"]      = delay;
    return obj;
}

Link Link::fromJson(const QJsonObject &obj)
{
    Link l;
    l.id         = obj["id"].toString();
    l.device1Id  = obj["device1Id"].toString();
    l.interface1 = obj["interface1"].toString();
    l.device2Id  = obj["device2Id"].toString();
    l.interface2 = obj["interface2"].toString();
    l.bandwidth  = obj["bandwidth"].toInt(1000);
    l.delay      = obj["delay"].toInt(1);
    return l;
}
