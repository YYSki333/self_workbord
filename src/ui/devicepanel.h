#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

struct DeviceEntry {
    int handle = -1;
    int linChCount = 2;
    QString label;
};

/** 侧边栏：设备扫描 / 打开 / 选择（不含协议配置）。 */
class DevicePanel : public QWidget
{
    Q_OBJECT
public:
    explicit DevicePanel(QWidget *parent = nullptr);

    void setBusy(bool busy);
    void clearDevices();
    void setDevices(const QVector<DeviceEntry> &devices);
    QVector<DeviceEntry> devices() const { return m_devices; }
    int currentHandle() const;
    bool hasDevice() const;
    void setInfoText(const QString &text);

signals:
    void scanRequested();
    void closeRequested();
    void deviceSelected(int handle);

private:
    QPushButton *m_scan = nullptr;
    QPushButton *m_close = nullptr;
    QLabel *m_info = nullptr;
    QComboBox *m_combo = nullptr;
    QVector<DeviceEntry> m_devices;
};
