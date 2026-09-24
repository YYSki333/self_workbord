#pragma once

#include <QMainWindow>

class DeviceManager;
class DevicePanel;
class LinConfigPanel;
class LinController;
class LinSendPanel;
class TracePanel;
class QStackedWidget;

// 完整类型在 cpp 中 include；此处前置声明不够用于槽参数
#include "ui/linsendpanel.h"

/**
 * 应用外壳：
 * - 左栏 = DevicePanel + 协议配置栈（LIN，后续可插 CAN/PWM）
 * - 主区 = TracePanel + 发送栈（LinSendPanel：单条 + 列表/循环）
 * 业务在 DeviceManager / LinController，窗口只接线。
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

private:
    void buildUi();
    void syncDeviceUi();
    void appendLocalFrame(bool isTx, int ch, const QString &type,
                          int id, unsigned char pid, int dlc,
                          const QString &dataPart, const QString &checkPart);
    void setStatus(const QString &text, int timeoutMs = 8000);

    DeviceManager *m_devices = nullptr;
    LinController *m_lin = nullptr;
    DevicePanel *m_devicePanel = nullptr;
    LinConfigPanel *m_linConfig = nullptr;
    TracePanel *m_trace = nullptr;
    LinSendPanel *m_linSend = nullptr;
    QStackedWidget *m_protocolStack = nullptr;
    QStackedWidget *m_sendStack = nullptr;
    bool m_linRunning = false;
};
