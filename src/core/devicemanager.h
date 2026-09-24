#pragma once

#include "core/traceframe.h"
#include "ui/devicepanel.h"

#include <QObject>
#include <QVector>

#include "usb_device.h"

/**
 * 设备生命周期：扫描 / 打开 / 关闭。
 * 协议控制器只依赖已打开的 handle，不重复扫设备。
 */
class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    void scan();
    void closeAll();
    bool isOpen(int handle) const;
    QVector<int> openHandles() const { return m_open; }
    QVector<DeviceEntry> entries() const { return m_entries; }

signals:
    void devicesChanged(const QVector<DeviceEntry> &entries);
    void statusMessage(const QString &text, int timeoutMs);

private:
    static int guessLinChannelCount(int handle);
    static QString formatSerial(const int serial[3]);

    QVector<int> m_open;
    QVector<DeviceEntry> m_entries;
};
