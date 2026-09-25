#pragma once

#include <QMainWindow>

class DeviceManager;
class DevicePanel;
class LinConfigPanel;
class LinController;
class LinSendPanel;
class TracePanel;
class QListWidget;
class QStackedWidget;
class QLabel;

#include "ui/linsendpanel.h"

/**
 * 左侧导航（分组子菜单）+ 右侧页面栈：
 * 图莫斯 → 设备扫描、LIN 总线、英迪芯标定；其它 → J-Link 烧录。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onStartLin();
    void onStopLin();
    void onWriteLin();
    void onReadLin();
    void onWriteListLine(int id, unsigned char check, const QString &dataHex);
    void onReadListLine(int id, unsigned char check);
    void onListItem(const LinSendPanel::ListItem &item);
    void onNavRowChanged(int row);

private:
    enum PageId {
        PageDevice = 0,
        PageLin = 1,
        PageIndieCal = 2,
        PageJlink = 3,
    };
    // nav row → page（含不可选分组标题行）
    enum NavRow {
        RowTomHeader = 0,
        RowDevice = 1,
        RowLin = 2,
        RowIndieCal = 3,
        RowOtherHeader = 4,
        RowJlink = 5,
    };

    void buildUi();
    QWidget *buildDevicePage();
    QWidget *buildLinPage();
    QWidget *buildIndieCalPage();
    QWidget *buildJlinkPage();
    void syncDeviceUi();
    void updateDeviceBadge();
    void appendLocalFrame(bool isTx, int ch, const QString &type,
                          int id, unsigned char pid, int dlc,
                          const QString &dataPart, const QString &checkPart);
    void setStatus(const QString &text, int timeoutMs = 8000);

    DeviceManager *m_devices = nullptr;
    LinController *m_lin = nullptr;
    DevicePanel *m_devicePanel = nullptr;
    QListWidget *m_nav = nullptr;
    QStackedWidget *m_pages = nullptr;
    LinConfigPanel *m_linConfig = nullptr;
    TracePanel *m_trace = nullptr;
    LinSendPanel *m_linSend = nullptr;
    QLabel *m_deviceBadge = nullptr;
    bool m_linRunning = false;
    bool m_navGuard = false;
};
