#pragma once

#include <QString>
#include <QStringList>

namespace linutil {

inline unsigned char calcPid(unsigned char id)
{
    id = static_cast<unsigned char>(id & 0x3F);
    const unsigned char p0 = static_cast<unsigned char>(
        (((id) ^ (id >> 1) ^ (id >> 2) ^ (id >> 4)) & 0x01) << 6);
    const unsigned char p1 = static_cast<unsigned char>(
        ((~((id >> 1) ^ (id >> 3) ^ (id >> 4) ^ (id >> 5))) & 0x01) << 7);
    return static_cast<unsigned char>(id | p0 | p1);
}

/** LIN 校验（含进位折叠）；enhanced=true 时加入 PID。 */
inline unsigned char linChecksum(bool enhanced, unsigned char pid,
                                 const unsigned char *data, int len)
{
    unsigned int sum = 0;
    if (enhanced)
        sum += pid;
    for (int i = 0; i < len; ++i) {
        sum += data[i];
        if (sum > 0xFF)
            sum = (sum & 0xFF) + 1; // fold carry
    }
    return static_cast<unsigned char>(0xFF - (sum & 0xFF));
}

/**
 * 解析十六进制字节串。
 * 接受：空格/逗号/冒号分隔，可选 0x 前缀，大小写不敏感。
 * 也接受无分隔连续串（长度为偶数，按字节对拆）。
 */
inline bool parseHexBytes(const QString &text, unsigned char *out,
                          int *outLen, int maxLen)
{
    QString t = text.trimmed();
    if (t.isEmpty())
        return false;
    t.replace(QLatin1Char(','), QLatin1Char(' '));
    t.replace(QLatin1Char(';'), QLatin1Char(' '));
    t.replace(QLatin1Char(':'), QLatin1Char(' '));
    t.replace(QLatin1Char('\t'), QLatin1Char(' '));

    // 无分隔连续 hex → 按对拆
    if (!t.contains(QLatin1Char(' '))) {
        QString hex = t;
        if (hex.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            hex = hex.mid(2);
        if (hex.isEmpty() || hex.size() % 2 != 0 || hex.size() / 2 > maxLen)
            return false;
        for (int i = 0; i < hex.size(); i += 2) {
            bool ok = false;
            const uint v = hex.mid(i, 2).toUInt(&ok, 16);
            if (!ok || v > 0xFF)
                return false;
            out[i / 2] = static_cast<unsigned char>(v);
        }
        *outLen = hex.size() / 2;
        return *outLen > 0;
    }

    const QStringList parts =
        t.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty() || parts.size() > maxLen)
        return false;
    int n = 0;
    for (QString p : parts) {
        p = p.trimmed();
        if (p.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            p = p.mid(2);
        bool ok = false;
        const uint v = p.toUInt(&ok, 16);
        if (!ok || v > 0xFF)
            return false;
        out[n++] = static_cast<unsigned char>(v);
    }
    *outLen = n;
    return n > 0;
}

inline QString formatBytes(const unsigned char *data, int len)
{
    QStringList parts;
    for (int i = 0; i < len; ++i)
        parts << QStringLiteral("%1").arg(data[i], 2, 16, QChar('0'));
    return parts.join(QLatin1Char(' '));
}

inline QString formatHex2(unsigned char v)
{
    return QStringLiteral("%1").arg(v, 2, 16, QChar('0')).toUpper();
}

} // namespace linutil
