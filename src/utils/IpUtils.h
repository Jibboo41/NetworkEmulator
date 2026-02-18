#pragma once
#include <QString>
#include <climits>

namespace IpUtils {

inline quint32 parse(const QString &ip)
{
    const QStringList parts = ip.split('.');
    if (parts.size() != 4) return 0;
    quint32 result = 0;
    for (int i = 0; i < 4; ++i)
        result = (result << 8) | (parts[i].toUInt() & 0xFF);
    return result;
}

inline QString format(quint32 ip)
{
    return QString("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xFF)
        .arg((ip >> 16) & 0xFF)
        .arg((ip >> 8)  & 0xFF)
        .arg(ip         & 0xFF);
}

inline quint32 networkAddress(quint32 ip, quint32 mask)
{
    return ip & mask;
}

inline int maskToPrefix(quint32 mask)
{
    int prefix = 0;
    while (mask & 0x80000000) { ++prefix; mask <<= 1; }
    return prefix;
}

inline quint32 prefixToMask(int prefix)
{
    if (prefix == 0) return 0;
    return ~((1u << (32 - prefix)) - 1);
}

inline bool isValidIp(const QString &ip)
{
    const QStringList parts = ip.split('.');
    if (parts.size() != 4) return false;
    for (const auto &p : parts) {
        bool ok;
        int v = p.toInt(&ok);
        if (!ok || v < 0 || v > 255) return false;
    }
    return true;
}

inline bool isValidMask(const QString &mask)
{
    if (!isValidIp(mask)) return false;
    quint32 m = parse(mask);
    // Valid masks have contiguous leading 1s
    quint32 inv = ~m;
    return ((inv + 1) & inv) == 0;
}

inline bool sameSubnet(const QString &ip1, const QString &ip2, const QString &mask)
{
    quint32 m = parse(mask);
    return networkAddress(parse(ip1), m) == networkAddress(parse(ip2), m);
}

} // namespace IpUtils
