#include "mainwindow.h"

#include "core/appversion.h"
#include "core/devicemanager.h"
#include "core/lincontroller.h"
#include "core/linutil.h"
#include "ui/devicepanel.h"
#include "ui/linconfigpanel.h"
#include "ui/linsendpanel.h"
#include "ui/tracepanel.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_devices = new DeviceManager(this);
    m_lin = new LinController(this);

    buildUi();

    connect(m_devices, &DeviceManager::devicesChanged, this,
            [this](const QVector<DeviceEntry> &) { syncDeviceUi(); });
    connect(m_devices, &DeviceManager::statusMessage, this,
            [this](const QString &t, int ms) { setStatus(t, ms); });

    connect(m_devicePanel, &DevicePanel::scanRequested, m_devices,
            &DeviceManager::scan);
    connect(m_devicePanel, &DevicePanel::closeRequested, this, [this]() {
        onStopLin();
        m_devices->closeAll();
        m_devicePanel->clearDevices();
        m_linConfig->setBusy(false);
        setStatus(tr("设备已关闭"));
    });
    connect(m_devicePanel, &DevicePanel::deviceSelected, this,
            [this](int) {
                for (const DeviceEntry &e : m_devicePanel->devices()) {
                    if (e.handle == m_devicePanel->currentHandle()) {
                        m_linConfig->setChannelCount(e.linChCount);
                        break;
                    }
                }
            });

    connect(m_linConfig, &LinConfigPanel::startRequested, this,
            &MainWindow::onStartLin);
    connect(m_linConfig, &LinConfigPanel::stopRequested, this,
            &MainWindow::onStopLin);

    connect(m_linSend, &LinSendPanel::writeRequested, this,
            &MainWindow::onWriteLin);
    connect(m_linSend, &LinSendPanel::readRequested, this,
            &MainWindow::onReadLin);
    connect(m_linSend, &LinSendPanel::statusText, this,
            [this](const QString &t) { setStatus(t); });

    connect(m_linSend, &LinSendPanel::listItemRequested, this,
            &MainWindow::onListItem);

    connect(m_lin, &LinController::framesCaptured, m_trace,
            &TracePanel::appendFrames);
    connect(m_lin, &LinController::errorOccurred, this,
            [this](const QString &m) {
                QMessageBox::warning(this, tr("警告"), m);
            });
    connect(m_lin, &LinController::stopped, this, [this]() {
        m_linRunning = false;
        m_linConfig->setRunning(false, tr("未启动"));
        m_linSend->setEnabledForRun(false, true);
        m_devicePanel->setBusy(false);
    });

    setStatus(tr("请扫描设备，配置 LIN 后启动；可用「列表发送」多条/循环。"));
}

MainWindow::~MainWindow()
{
    m_lin->stop();
    m_devices->closeAll();
}

void MainWindow::buildUi()
{
    auto *central = new QWidget;
    auto *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *side = new QWidget;
    side->setObjectName(QStringLiteral("sidebar"));
    side->setMinimumWidth(260);
    side->setMaximumWidth(300);
    side->setStyleSheet(QStringLiteral(
        "QWidget#sidebar { border-right: 1px solid palette(mid); }"));
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(12, 12, 12, 12);
    sideLayout->setSpacing(8);

    m_devicePanel = new DevicePanel;
    sideLayout->addWidget(m_devicePanel);

    auto *sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QStringLiteral("background:palette(mid);"));
    sideLayout->addWidget(sep);

    m_protocolStack = new QStackedWidget;
    m_linConfig = new LinConfigPanel;
    m_protocolStack->addWidget(m_linConfig);
    sideLayout->addWidget(m_protocolStack);
    sideLayout->addStretch();

    auto *center = new QVBoxLayout;
    center->setContentsMargins(0, 0, 0, 0);
    center->setSpacing(0);

    m_trace = new TracePanel;
    center->addWidget(m_trace, 1);

    m_sendStack = new QStackedWidget;
    m_linSend = new LinSendPanel;
    m_sendStack->addWidget(m_linSend);
    center->addWidget(m_sendStack);

    mainLayout->addWidget(side);
    auto *right = new QWidget;
    right->setLayout(center);
    mainLayout->addWidget(right, 1);

    setCentralWidget(central);
    setWindowTitle(tr("tomoss — LIN Trace · v%1")
                       .arg(QString::fromLatin1(version::semver())));
    resize(1100, 720);
}

void MainWindow::syncDeviceUi()
{
    m_devicePanel->setDevices(m_devices->entries());
    if (m_devicePanel->hasDevice()) {
        m_devicePanel->setInfoText(
            tr("已打开 %1 台").arg(m_devicePanel->devices().size()));
        for (const DeviceEntry &e : m_devicePanel->devices()) {
            if (e.handle == m_devicePanel->currentHandle()) {
                m_linConfig->setChannelCount(e.linChCount);
                break;
            }
        }
    } else {
        m_devicePanel->setInfoText(tr("未发现设备"));
        m_linConfig->setBusy(false);
    }
}

void MainWindow::onStartLin()
{
    if (m_linRunning)
        return;
    if (!m_devicePanel->hasDevice())
        m_devicePanel->setDevices(m_devices->entries());

    const int handle = m_devicePanel->currentHandle();
    if (handle < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先扫描并选择设备。"));
        return;
    }

    const int ch = m_linConfig->channel();
    const int baud = m_linConfig->baudBps();
    const bool master = m_linConfig->isMaster();

    if (m_lin->start(handle, ch, baud, master) != 0)
        return;

    m_linRunning = true;
    m_devicePanel->setBusy(true);
    m_linConfig->setRunning(
        true, tr("运行中 · LIN%1 · %2 · %3 bps")
                  .arg(ch + 1)
                  .arg(master ? tr("主机") : tr("从机"))
                  .arg(baud));
    m_linSend->setEnabledForRun(true, master);
    setStatus(tr("LIN 已启动"));
}

void MainWindow::onStopLin()
{
    m_lin->stop();
    m_linRunning = false;
    m_linConfig->setRunning(false, tr("未启动"));
    m_linSend->setEnabledForRun(false, true);
    m_devicePanel->setBusy(false);
}

void MainWindow::appendLocalFrame(bool isTx, int ch, const QString &type,
                                  int id, unsigned char pid, int dlc,
                                  const QString &dataPart,
                                  const QString &checkPart)
{
    TraceFrame f;
    f.epochMs = QDateTime::currentMSecsSinceEpoch();
    f.isTx = isTx;
    f.channel = ch;
    f.bus = QStringLiteral("LIN");
    f.direction = isTx ? QStringLiteral("TX") : QStringLiteral("RX");
    f.type = type;
    f.idText = linutil::formatHex2(static_cast<unsigned char>(id));
    f.pidText = linutil::formatHex2(pid);
    f.dlcText = QString::number(dlc);
    f.dataText = dataPart;
    f.checkText = checkPart;
    m_trace->appendFrames({f});
}

void MainWindow::onWriteLin()
{
    if (!m_linRunning || !m_linConfig->isMaster())
        return;
    onWriteListLine(m_linSend->sendId(), m_linSend->checkType(),
                    m_linSend->dataHex());
}

void MainWindow::onListItem(const LinSendPanel::ListItem &item)
{
    if (!m_linRunning || !m_linConfig->isMaster())
        return;
    if (item.direction == LinSendPanel::DirRead)
        onReadListLine(item.id, item.checkType);
    else
        onWriteListLine(item.id, item.checkType, item.dataHex);
}

void MainWindow::onWriteListLine(int id, unsigned char check,
                                 const QString &dataHex)
{
    if (!m_linRunning || !m_linConfig->isMaster())
        return;

    unsigned char data[8] = {};
    int len = 0;
    if (!linutil::parseHexBytes(dataHex, data, &len, 8)) {
        setStatus(tr("Data 非法，跳过：%1").arg(dataHex), 5000);
        return;
    }

    const int handle = m_devicePanel->currentHandle();
    const int ch = m_linConfig->channel();
    const unsigned char uid = static_cast<unsigned char>(id);
    const int ret = m_lin->write(handle, ch, uid, data, len, check);
    if (ret != LIN_EX_SUCCESS) {
        setStatus(tr("主机写失败 ID=0x%1（%2）")
                      .arg(linutil::formatHex2(uid))
                      .arg(ret),
                  5000);
        return;
    }

    const unsigned char pid = linutil::calcPid(uid);
    const unsigned char ck =
        linutil::linChecksum(check == 1, pid, data, len);
    appendLocalFrame(true, ch, tr("主机写 MW"), id, pid, len,
                     linutil::formatBytes(data, len),
                     QStringLiteral("%1 %2")
                         .arg(linutil::formatHex2(ck))
                         .arg(check == 1 ? tr("增强") : tr("标准")));
    setStatus(tr("TX ID=0x%1 PID=0x%2 Data=%3 CK=%4 %5")
                  .arg(linutil::formatHex2(uid))
                  .arg(linutil::formatHex2(pid))
                  .arg(linutil::formatBytes(data, len))
                  .arg(linutil::formatHex2(ck))
                  .arg(check == 1 ? tr("增强") : tr("标准")));
}

void MainWindow::onReadListLine(int id, unsigned char check)
{
    if (!m_linRunning || !m_linConfig->isMaster())
        return;

    const int handle = m_devicePanel->currentHandle();
    const int ch = m_linConfig->channel();
    const unsigned char uid = static_cast<unsigned char>(id);
    unsigned char data[8] = {};
    const int n = m_lin->read(handle, ch, uid, data, 8);
    if (n < 0) {
        setStatus(tr("主机读失败 ID=0x%1（%2）")
                      .arg(linutil::formatHex2(uid))
                      .arg(n),
                  5000);
        return;
    }

    const unsigned char pid = linutil::calcPid(uid);
    const QString body = n == 0 ? QStringLiteral("(无从机响应)")
                                : linutil::formatBytes(data, n);
    const unsigned char ck =
        n > 0 ? linutil::linChecksum(check == 1, pid, data, n) : 0;
    const QString checkPart =
        n > 0 ? QStringLiteral("%1 %2")
                     .arg(linutil::formatHex2(ck))
                     .arg(check == 1 ? tr("增强") : tr("标准"))
              : QStringLiteral("—");
    appendLocalFrame(false, ch, tr("主机读 MR"), id, pid, n, body, checkPart);
    setStatus(n == 0
                  ? tr("RX ID=0x%1 无从机响应")
                        .arg(linutil::formatHex2(uid))
                  : tr("RX ID=0x%1 Data=%2")
                        .arg(linutil::formatHex2(uid))
                        .arg(body));
}

void MainWindow::onReadLin()
{
    if (!m_linRunning || !m_linConfig->isMaster())
        return;
    onReadListLine(m_linSend->sendId(), m_linSend->checkType());
}

void MainWindow::setStatus(const QString &text, int timeoutMs)
{
    statusBar()->showMessage(text, timeoutMs);
}
