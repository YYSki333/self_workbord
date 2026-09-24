#include "core/devicemanager.h"

#include <QString>
#include <QStringList>

namespace {
constexpr int kMaxDevices = 32;
}

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
{
}

DeviceManager::~DeviceManager()
{
    closeAll();
}

void DeviceManager::scan()
{
    closeAll();
    m_entries.clear();

    int handles[kMaxDevices] = {};
    const int devNum = USB_ScanDevice(handles);
    if (devNum <= 0) {
        emit devicesChanged(m_entries);
        emit statusMessage(tr("未扫描到设备：请检查 USB 连接与驱动。"),
                           5000);
        return;
    }

    int openCount = 0;
    for (int i = 0; i < devNum && i < kMaxDevices; ++i) {
        const int handle = handles[i];
        if (USB_OpenDevice(handle) != 1)
            continue;
        m_open.append(handle);
        ++openCount;

        DEVICE_INFO info = {};
        char funcStr[256] = {};
        QString name = tr("设备");
        QString extra;
        int linCh = guessLinChannelCount(handle);

        if (DEV_GetDeviceInfo(handle, &info, funcStr) == 1
            && info.FirmwareName[0] != '\0') {
            name = QString::fromLatin1(info.FirmwareName).trimmed();
            const QString fw =
                QStringLiteral("%1.%2")
                    .arg(info.FirmwareVersion / 100)
                    .arg(info.FirmwareVersion % 100, 2, 10, QChar('0'));
            extra = tr("FW %1 · SN %2")
                        .arg(fw)
                        .arg(formatSerial(info.SerialNumber));
        }

        HARDWARE_INFO hw = {};
        if (DEV_GetHardwareInfo(handle, &hw) == 1
            && hw.LINChannelNum > 0 && hw.LINChannelNum <= 4) {
            linCh = hw.LINChannelNum;
        }
        extra += QStringLiteral(" · LIN×%1").arg(linCh);

        DeviceEntry e;
        e.handle = handle;
        e.linChCount = linCh;
        e.label = QStringLiteral("0x%1 — %2")
                      .arg(QString::number(handle, 16)
                               .rightJustified(8, QLatin1Char('0'))
                               .toUpper())
                      .arg(name);
        if (!extra.isEmpty())
            e.label += QStringLiteral(" · ") + extra;
        m_entries.append(e);
    }

    emit devicesChanged(m_entries);
    emit statusMessage(
        tr("扫描完成：发现 %1 台，打开 %2 台").arg(devNum).arg(openCount),
        5000);
}

void DeviceManager::closeAll()
{
    for (const int h : m_open)
        USB_CloseDevice(h);
    m_open.clear();
    m_entries.clear();
}

bool DeviceManager::isOpen(int handle) const
{
    return m_open.contains(handle);
}

int DeviceManager::guessLinChannelCount(int handle)
{
    const QString prefix =
        QString::number(handle, 16)
            .rightJustified(8, QLatin1Char('0'))
            .left(2)
            .toUpper();
    if (prefix == QLatin1String("41"))
        return 1;
    if (prefix == QLatin1String("46") || prefix == QLatin1String("54")
        || prefix == QLatin1String("55"))
        return 4;
    return 2;
}

QString DeviceManager::formatSerial(const int serial[3])
{
    return QString("%1%2%3")
        .arg(serial[0], 8, 16, QChar('0'))
        .arg(serial[1], 8, 16, QChar('0'))
        .arg(serial[2], 8, 16, QChar('0'))
        .toUpper();
}
