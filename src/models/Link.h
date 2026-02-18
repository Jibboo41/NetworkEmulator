#pragma once
#include <QString>
#include <QJsonObject>

struct Link {
    QString id;
    QString device1Id;
    QString interface1; // interface name on device1
    QString device2Id;
    QString interface2; // interface name on device2
    int     bandwidth = 1000; // Mbps
    int     delay     = 1;    // ms

    QJsonObject toJson() const;
    static Link fromJson(const QJsonObject &obj);
};
