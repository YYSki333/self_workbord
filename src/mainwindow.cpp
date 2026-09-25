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
#include <QListWidget>
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
        updateDeviceBadge();
        setStatus(tr("设备已关闭"));
    });
    connect(m_devicePanel, &DevicePanel::deviceSelected, this, [this](int) {
        for (const DeviceEntry &e : m_devicePanel->devices()) {
            if (e.handle == m_devicePanel->currentHandle()) {
                m_linConfig->setChannelCount(e.linChCount);
                break;
            }
        }
        updateDeviceBadge();
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

    setStatus(tr("图莫斯 → 设备扫描 / LIN / 英迪芯标定；其它 → J-Link。"));
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
    side->setMinimumWidth(230);
    side->setMaximumWidth(280);
    side->setStyleSheet(QStringLiteral(
        "QWidget#sidebar { border-right: 1px solid palette(mid); }"));
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(10, 12, 10, 12);
    sideLayout->setSpacing(4);

    auto makeHeader = [](const QString &text) {
        auto *item = new QListWidgetItem(text);
        item->setFlags(Qt::ItemIsEnabled);
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
        return item;
    };

    // 图莫斯 → 设备扫描 / LIN / 英迪芯标定；其它 → J-Link
    m_nav = new QListWidget;
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->addItem(makeHeader(tr("图莫斯")));
    m_nav->addItem(tr("  设备扫描"));
    m_nav->addItem(tr("  LIN 总线"));
    m_nav->addItem(tr("  英迪芯标定"));
    m_nav->addItem(makeHeader(tr("其它")));
    m_nav->addItem(tr("  J-Link 烧录"));
    m_nav->setCurrentRow(RowDevice);
    sideLayout->addWidget(m_nav, 1);

    // —— 页面栈 ——
    m_pages = new QStackedWidget;
    m_pages->addWidget(buildDevicePage());
    m_pages->addWidget(buildLinPage());
    m_pages->addWidget(buildIndieCalPage());
    m_pages->addWidget(buildJlinkPage());

    mainLayout->addWidget(side);
    mainLayout->addWidget(m_pages, 1);

    setCentralWidget(central);
    setWindowTitle(tr("tomoss — 设备扫描 · v%1")
                       .arg(QString::fromLatin1(version::semver())));
    resize(1180, 740);

    connect(m_nav, &QListWidget::currentRowChanged, this,
            &MainWindow::onNavRowChanged);
    // 初始页
    m_pages->setCurrentIndex(PageDevice);
}

QWidget *MainWindow::buildDevicePage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(12);

    auto *title = new QLabel(tr("图莫斯 · 设备扫描"));
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 3);
    f.setBold(true);
    title->setFont(f);
    lay->addWidget(title);

    auto *hint = new QLabel(
        tr("扫描并打开 USB2XXX 适配器；选中设备后可在「LIN 总线」页使用。"));
    hint->setWordWrap(true);
    lay->addWidget(hint);

    m_devicePanel = new DevicePanel;
    // 页面内展示：加最大宽度，避免控件拉满
    auto *wrap = new QWidget;
    auto *wrapLay = new QVBoxLayout(wrap);
    wrapLay->setContentsMargins(0, 8, 0, 0);
    wrapLay->addWidget(m_devicePanel);
    wrapLay->addStretch();
    wrap->setMaximumWidth(480);
    lay->addWidget(wrap, 0, Qt::AlignLeft);
    lay->addStretch();

    return page;
}

QWidget *MainWindow::buildLinPage()
{
    auto *page = new QWidget;
    auto *lay = new QHBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *left = new QVBoxLayout;
    left->setContentsMargins(8, 8, 8, 8);
    left->setSpacing(6);

    m_deviceBadge = new QLabel;
    m_deviceBadge->setWordWrap(true);
    m_deviceBadge->setStyleSheet(QStringLiteral("font-size:12px;"));
    left->addWidget(m_deviceBadge);

    m_linConfig = new LinConfigPanel;
    m_linConfig->setMinimumWidth(200);
    m_linConfig->setMaximumWidth(250);
    left->addWidget(m_linConfig);
    left->addStretch();

    auto *leftWrap = new QWidget;
    leftWrap->setLayout(left);
    leftWrap->setMinimumWidth(220);
    leftWrap->setMaximumWidth(270);
    lay->addWidget(leftWrap);

    auto *sep = new QWidget;
    sep->setFixedWidth(1);
    sep->setStyleSheet(QStringLiteral("background:palette(mid);"));
    lay->addWidget(sep);

    auto *center = new QVBoxLayout;
    center->setContentsMargins(0, 0, 0, 0);
    center->setSpacing(0);
    m_trace = new TracePanel;
    center->addWidget(m_trace, 1);
    m_linSend = new LinSendPanel;
    center->addWidget(m_linSend);

    auto *right = new QWidget;
    right->setLayout(center);
    lay->addWidget(right, 1);

    return page;
}

QWidget *MainWindow::buildIndieCalPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(32, 32, 32, 32);
    lay->setSpacing(12);

    auto *title = new QLabel(tr("英迪芯标定"));
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    title->setFont(f);
    lay->addWidget(title);

    auto *hint = new QLabel(
        tr("该子页面规划中：英迪芯（Indie）传感器标定流程。\n"
           "属于「图莫斯」分类，与设备扫描 / LIN 共用已打开的适配器。"));
    hint->setWordWrap(true);
    lay->addWidget(hint);
    lay->addStretch();
    return page;
}

QWidget *MainWindow::buildJlinkPage()
{
    auto *page = new QWidget;
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(32, 32, 32, 32);
    lay->setSpacing(12);

    auto *title = new QLabel(tr("J-Link 烧录"));
    QFont f = title->font();
    f.setPointSize(f.pointSize() + 4);
    f.setBold(true);
    title->setFont(f);
    lay->addWidget(title);

    auto *hint = new QLabel(
        tr("该子页面规划中：连接 J-Link、选择目标 MCU、烧录 ELF/BIN 等。\n"
           "与图莫斯设备/LIN 无关，通过左侧「其它」导航进入。"));
    hint->setWordWrap(true);
    lay->addWidget(hint);
    lay->addStretch();
    return page;
}

void MainWindow::onNavRowChanged(int row)
{
    if (m_navGuard)
        return;
    // 分组标题不可选，但若被选中则弹回
    if (row == RowTomHeader || row == RowOtherHeader) {
        m_navGuard = true;
        m_nav->setCurrentRow(row == RowTomHeader ? RowDevice : RowJlink);
        m_navGuard = false;
        return;
    }

    if (row == RowDevice) {
        m_pages->setCurrentIndex(PageDevice);
        setWindowTitle(tr("tomoss — 设备扫描 · v%1")
                           .arg(QString::fromLatin1(version::semver())));
        setStatus(tr("图莫斯 · 设备扫描"));
    } else if (row == RowLin) {
        m_pages->setCurrentIndex(PageLin);
        updateDeviceBadge();
        setWindowTitle(tr("tomoss — LIN · v%1")
                           .arg(QString::fromLatin1(version::semver())));
        setStatus(tr("图莫斯 · LIN 总线"));
    } else if (row == RowIndieCal) {
        m_pages->setCurrentIndex(PageIndieCal);
        setWindowTitle(tr("tomoss — 英迪芯标定 · v%1")
                           .arg(QString::fromLatin1(version::semver())));
        setStatus(tr("图莫斯 · 英迪芯标定（开发中）"));
    } else if (row == RowJlink) {
        m_pages->setCurrentIndex(PageJlink);
        setWindowTitle(tr("tomoss — J-Link · v%1")
                           .arg(QString::fromLatin1(version::semver())));
        setStatus(tr("其它 · J-Link 烧录（开发中）"));
    }
}

void MainWindow::updateDeviceBadge()
{
    if (!m_deviceBadge)
        return;
    if (!m_devicePanel->hasDevice()) {
        m_deviceBadge->setText(
            tr("设备：未扫描 — 请到「图莫斯 → 设备扫描」"));
        return;
    }
    const int h = m_devicePanel->currentHandle();
    m_deviceBadge->setText(
        tr("设备：0x%1（可在设备扫描页切换）")
            .arg(QString::number(h, 16)
                     .rightJustified(8, QLatin1Char('0'))
                     .toUpper()));
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
    updateDeviceBadge();
}

void MainWindow::onStartLin()
{
    if (m_linRunning)
        return;
    if (!m_devicePanel->hasDevice())
        m_devicePanel->setDevices(m_devices->entries());

    const int handle = m_devicePanel->currentHandle();
    if (handle < 0) {
        QMessageBox::warning(
            this, tr("提示"),
            tr("请先在「图莫斯 → 设备扫描」中扫描并选择设备。"));
        m_navGuard = true;
        m_nav->setCurrentRow(RowDevice);
        m_navGuard = false;
        onNavRowChanged(RowDevice);
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
