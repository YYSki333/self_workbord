#include "ui/linsendpanel.h"

#include "core/linutil.h"
#include "ui/uifont.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
enum Col { ColEn = 0, ColDir, ColId, ColCheck, ColData, ColCycle };

const auto kOrg = QStringLiteral("toomoss");
const auto kApp = QStringLiteral("tomoss");
const auto kArr = QStringLiteral("linSendList");

QString dirName(int dir)
{
    return dir == LinSendPanel::DirRead ? QObject::tr("读")
                                        : QObject::tr("写");
}
} // namespace

LinSendPanel::LinSendPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();

    m_cycleTimer = new QTimer(this);
    m_cycleTimer->setTimerType(Qt::PreciseTimer);
    connect(m_cycleTimer, &QTimer::timeout, this,
            &LinSendPanel::onCycleTick);

    connect(m_add, &QPushButton::clicked, this, &LinSendPanel::onAddRow);
    connect(m_del, &QPushButton::clicked, this,
            &LinSendPanel::onRemoveRows);
    connect(m_sendOnce, &QPushButton::clicked, this,
            &LinSendPanel::onSendOnce);
    connect(m_cycle, &QPushButton::clicked, this,
            &LinSendPanel::onToggleCycle);

    loadList();
    updateListUi();
}

LinSendPanel::~LinSendPanel()
{
    if (m_cycleTimer)
        m_cycleTimer->stop();
    saveList();
}

void LinSendPanel::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 4, 8, 6);
    root->setSpacing(6);

    auto *single = new QHBoxLayout;
    single->setSpacing(8);
    single->addWidget(new QLabel(tr("单条 ID")));
    m_id = new QSpinBox;
    m_id->setRange(0, 63);
    m_id->setValue(1);
    m_id->setPrefix(QStringLiteral("0x"));
    m_id->setDisplayIntegerBase(16);
    uifont::applyMono(m_id, 10);
    single->addWidget(m_id);

    single->addWidget(new QLabel(tr("校验")));
    m_check = new QComboBox;
    m_check->addItem(tr("标准校验"));
    m_check->addItem(tr("增强校验"));
    single->addWidget(m_check);

    single->addWidget(new QLabel(tr("Data")));
    m_data = new QLineEdit(QStringLiteral("01 02 03 04"));
    m_data->setPlaceholderText(tr("空格分隔 hex，最多 8 字节"));
    uifont::applyMono(m_data, 10);
    single->addWidget(m_data, 1);

    m_write = new QPushButton(tr("发送 (主机写)"));
    m_read = new QPushButton(tr("读取 (主机读)"));
    m_write->setEnabled(false);
    m_read->setEnabled(false);
    single->addWidget(m_write);
    single->addWidget(m_read);
    root->addLayout(single);

    connect(m_write, &QPushButton::clicked, this,
            &LinSendPanel::writeRequested);
    connect(m_read, &QPushButton::clicked, this,
            &LinSendPanel::readRequested);

    auto *listBar = new QHBoxLayout;
    auto *listTitle = new QLabel(tr("列表发送"));
    QFont bold = listTitle->font();
    bold.setBold(true);
    listTitle->setFont(bold);
    listBar->addWidget(listTitle);
    listBar->addStretch();

    listBar->addWidget(new QLabel(tr("默认方向")));
    m_defaultDir = new QComboBox;
    m_defaultDir->addItem(tr("写"));
    m_defaultDir->addItem(tr("读"));
    listBar->addWidget(m_defaultDir);

    listBar->addWidget(new QLabel(tr("默认周期(ms)")));
    m_defaultCycle = new QSpinBox;
    m_defaultCycle->setRange(10, 60000);
    m_defaultCycle->setValue(100);
    listBar->addWidget(m_defaultCycle);

    m_add = new QPushButton(tr("添加"));
    m_del = new QPushButton(tr("删除选中"));
    m_sendOnce = new QPushButton(tr("发送"));
    m_cycle = new QPushButton(tr("循环发送"));
    m_sendOnce->setEnabled(false);
    m_cycle->setEnabled(false);
    listBar->addWidget(m_add);
    listBar->addWidget(m_del);
    listBar->addWidget(m_sendOnce);
    listBar->addWidget(m_cycle);
    root->addLayout(listBar);

    m_table = new QTableWidget;
    uifont::applyMono(m_table, 10);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {tr("使能"), tr("方向"), tr("ID (hex)"), tr("校验"), tr("Data"),
         tr("周期 ms")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    auto *hdr = m_table->horizontalHeader();
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setColumnWidth(ColEn, 50);
    m_table->setColumnWidth(ColDir, 56);
    m_table->setColumnWidth(ColId, 64);
    m_table->setColumnWidth(ColCheck, 64);
    hdr->setSectionResizeMode(ColData, QHeaderView::Stretch);
    m_table->setColumnWidth(ColCycle, 72);
    root->addWidget(m_table, 1);

    auto *bottom = new QHBoxLayout;
    m_listState = new QLabel;
    m_listState->setStyleSheet(QStringLiteral("font-size:11px;"));
    bottom->addWidget(m_listState, 1);
    auto *hint = new QLabel(
        tr("方向：写=主机写，读=主机读；列表自动保存，重启后保留。"));
    hint->setStyleSheet(QStringLiteral("font-size:11px;"));
    bottom->addWidget(hint);
    root->addLayout(bottom);
}

void LinSendPanel::loadList()
{
    m_loading = true;
    QSettings s(kOrg, kApp);
    const int n = s.beginReadArray(kArr);
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        ListItem item;
        item.enabled = s.value(QStringLiteral("en"), true).toBool();
        item.direction = s.value(QStringLiteral("dir"), DirWrite).toInt()
                             == DirRead
                             ? DirRead
                             : DirWrite;
        item.id = s.value(QStringLiteral("id"), 1).toInt() & 0x3F;
        item.checkType = static_cast<unsigned char>(
            s.value(QStringLiteral("ck"), 0).toInt() & 0x3);
        item.dataHex = s.value(QStringLiteral("data")).toString();
        item.cycleMs = qBound(10, s.value(QStringLiteral("cycle"), 100).toInt(),
                              60000);

        unsigned char tmp[8];
        int len = 0;
        if (linutil::parseHexBytes(item.dataHex, tmp, &len, 8)) {
            item.dataHex = linutil::formatBytes(tmp, len);
        } else if (item.direction == DirRead) {
            item.dataHex.clear(); // 读方向可无 Data
        } else {
            continue; // 写方向必须有合法 Data
        }

        const int row = m_table->rowCount();
        m_table->insertRow(row);
        applyRow(row, item);
    }
    s.endArray();

    m_id->setValue(
        s.value(QStringLiteral("draft/id"), m_id->value()).toInt() & 0x3F);
    m_check->setCurrentIndex(qBound(
        0, s.value(QStringLiteral("draft/check"), m_check->currentIndex()).toInt(),
        1));
    m_data->setText(
        s.value(QStringLiteral("draft/data"), m_data->text()).toString());
    m_defaultDir->setCurrentIndex(
        s.value(QStringLiteral("draft/dir"), DirWrite).toInt() == DirRead ? 1
                                                                          : 0);
    m_defaultCycle->setValue(qBound(
        10, s.value(QStringLiteral("draft/cycle"), m_defaultCycle->value()).toInt(),
        60000));
    m_loading = false;
}

void LinSendPanel::saveList() const
{
    if (m_loading)
        return;
    QSettings s(kOrg, kApp);
    s.beginGroup(kArr);
    s.remove(QString());
    s.endGroup();

    s.beginWriteArray(kArr, m_table->rowCount());
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const ListItem item = rowToItem(r);
        s.setArrayIndex(r);
        s.setValue(QStringLiteral("en"), item.enabled);
        s.setValue(QStringLiteral("dir"), item.direction);
        s.setValue(QStringLiteral("id"), item.id & 0x3F);
        s.setValue(QStringLiteral("ck"), int(item.checkType));
        s.setValue(QStringLiteral("data"), item.dataHex);
        s.setValue(QStringLiteral("cycle"), item.cycleMs);
    }
    s.endArray();

    s.setValue(QStringLiteral("draft/id"), m_id->value());
    s.setValue(QStringLiteral("draft/check"), m_check->currentIndex());
    s.setValue(QStringLiteral("draft/data"), m_data->text());
    s.setValue(QStringLiteral("draft/dir"), m_defaultDir->currentIndex());
    s.setValue(QStringLiteral("draft/cycle"), m_defaultCycle->value());
    s.sync();
}

void LinSendPanel::updateListUi()
{
    const int n = m_table->rowCount();
    const bool canSend = m_running && m_master && n > 0;
    m_del->setEnabled(n > 0);
    m_sendOnce->setEnabled(canSend);
    m_cycle->setEnabled(canSend);
    if (n == 0)
        m_listState->setText(
            tr("列表为空：点「添加」把单条内容加入列表（自动保存）"));
    else if (m_cycleTimer->isActive())
        ;
    else
        m_listState->setText(tr("共 %1 条（已持久化）").arg(n));
}

void LinSendPanel::setEnabledForRun(bool running, bool master)
{
    m_running = running;
    m_master = master;
    const bool on = running && master;

    m_write->setEnabled(on);
    m_read->setEnabled(on);
    m_id->setEnabled(true);
    m_check->setEnabled(true);
    m_data->setEnabled(true);
    m_add->setEnabled(true);
    m_defaultCycle->setEnabled(true);
    m_defaultDir->setEnabled(true);
    updateListUi();

    if (!on && m_cycleTimer->isActive()) {
        m_cycleTimer->stop();
        m_cycleRow = -1;
        m_cycle->setText(tr("循环发送"));
        m_listState->setText(tr("循环已停止"));
    }
}

int LinSendPanel::sendId() const
{
    return m_id->value();
}

unsigned char LinSendPanel::checkType() const
{
    return static_cast<unsigned char>(m_check->currentIndex());
}

QString LinSendPanel::dataHex() const
{
    return m_data->text();
}

QList<LinSendPanel::ListItem> LinSendPanel::enabledItems() const
{
    QList<ListItem> out;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto *en = qobject_cast<QCheckBox *>(m_table->cellWidget(r, ColEn));
        if (en && en->isChecked())
            out.append(rowToItem(r));
    }
    return out;
}

bool LinSendPanel::isCycleRunning() const
{
    return m_cycleTimer->isActive();
}

void LinSendPanel::onAddRow()
{
    ListItem item;
    item.enabled = true;
    item.direction =
        m_defaultDir->currentIndex() == 1 ? DirRead : DirWrite;
    item.id = sendId() & 0x3F;
    item.checkType = checkType();
    item.cycleMs = m_defaultCycle->value();

    unsigned char tmp[8];
    int len = 0;
    if (item.direction == DirWrite) {
        if (!linutil::parseHexBytes(dataHex(), tmp, &len, 8)) {
            emit statusText(tr("写方向的 Data 须为最多 8 字节十六进制"));
            return;
        }
        item.dataHex = linutil::formatBytes(tmp, len);
        m_data->setText(item.dataHex);
    } else {
        // 读方向：Data 可选，仅作备注
        if (linutil::parseHexBytes(dataHex(), tmp, &len, 8))
            item.dataHex = linutil::formatBytes(tmp, len);
        else
            item.dataHex.clear();
    }

    const int row = m_table->rowCount();
    m_table->insertRow(row);
    applyRow(row, item);
    saveList();
    updateListUi();
    emit statusText(
        tr("已添加 %1 ID=0x%2 data=%3（%4 条）")
            .arg(dirName(item.direction))
            .arg(linutil::formatHex2(static_cast<unsigned char>(item.id)))
            .arg(item.dataHex.isEmpty() ? QStringLiteral("—") : item.dataHex)
            .arg(m_table->rowCount()));
}

void LinSendPanel::onRemoveRows()
{
    if (m_cycleTimer->isActive())
        onToggleCycle();

    const auto rows = m_table->selectionModel()->selectedRows();
    QVector<int> indices;
    for (const QModelIndex &idx : rows)
        indices.append(idx.row());
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    for (int r : indices)
        m_table->removeRow(r);

    saveList();
    updateListUi();
}

void LinSendPanel::onSendOnce()
{
    if (!m_running || !m_master)
        return;
    int sent = 0;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto *en = qobject_cast<QCheckBox *>(m_table->cellWidget(r, ColEn));
        if (!en || !en->isChecked())
            continue;
        emitItem(rowToItem(r));
        ++sent;
    }
    emit statusText(tr("已按列表执行 %1 条").arg(sent));
}

void LinSendPanel::onToggleCycle()
{
    if (!m_running || !m_master)
        return;

    if (m_cycleTimer->isActive()) {
        m_cycleTimer->stop();
        m_cycleRow = -1;
        m_cycle->setText(tr("循环发送"));
        m_listState->setText(
            tr("循环停止 · 共 %1 条").arg(m_table->rowCount()));
        emit cycleFinished();
        return;
    }

    if (nextEnabledRow(-1) < 0) {
        emit statusText(tr("列表中没有勾选使能的行"));
        return;
    }

    m_cycleRow = -1;
    m_cycle->setText(tr("停止循环"));
    m_listState->setText(tr("循环发送中…"));
    onCycleTick();
}

void LinSendPanel::onCycleTick()
{
    if (!m_running || !m_master) {
        m_cycleTimer->stop();
        return;
    }

    const int row = nextEnabledRow(m_cycleRow);
    if (row < 0) {
        m_cycleTimer->stop();
        m_cycleRow = -1;
        m_cycle->setText(tr("循环发送"));
        m_listState->setText(tr("循环结束（无使能行）"));
        emit cycleFinished();
        return;
    }

    const ListItem item = rowToItem(row);
    m_cycleRow = row;
    emitItem(item);
    m_listState->setText(
        tr("循环中 · 行 %1/%2 · %3 ID 0x%4 · %5 ms")
            .arg(row + 1)
            .arg(m_table->rowCount())
            .arg(dirName(item.direction))
            .arg(linutil::formatHex2(static_cast<unsigned char>(item.id)))
            .arg(item.cycleMs));

    if (!m_cycleTimer->isActive() || m_cycleTimer->interval() != item.cycleMs)
        m_cycleTimer->start(qMax(10, item.cycleMs));
}

void LinSendPanel::emitItem(const ListItem &item)
{
    emit listItemRequested(item);
}

LinSendPanel::ListItem LinSendPanel::rowToItem(int row) const
{
    ListItem item;
    auto *en = qobject_cast<QCheckBox *>(m_table->cellWidget(row, ColEn));
    item.enabled = en && en->isChecked();

    auto *dirCombo =
        qobject_cast<QComboBox *>(m_table->cellWidget(row, ColDir));
    item.direction = dirCombo && dirCombo->currentIndex() == 1 ? DirRead
                                                               : DirWrite;

    if (m_table->item(row, ColId)) {
        QString idText = m_table->item(row, ColId)->text().trimmed();
        if (idText.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
            idText = idText.mid(2);
        bool ok = false;
        const int id = idText.toInt(&ok, 16);
        item.id = ok ? (id & 0x3F) : 0;
    }
    auto *checkCombo =
        qobject_cast<QComboBox *>(m_table->cellWidget(row, ColCheck));
    item.checkType =
        checkCombo ? static_cast<unsigned char>(checkCombo->currentIndex()) : 0;
    if (m_table->item(row, ColData))
        item.dataHex = m_table->item(row, ColData)->text();
    if (m_table->item(row, ColCycle))
        item.cycleMs = m_table->item(row, ColCycle)->text().toInt();
    return item;
}

void LinSendPanel::applyRow(int row, const ListItem &item)
{
    auto *en = new QCheckBox;
    en->blockSignals(true);
    en->setChecked(item.enabled);
    en->setStyleSheet(QStringLiteral("margin-left:8px;"));
    m_table->setCellWidget(row, ColEn, en);
    en->blockSignals(false);
    connect(en, &QCheckBox::toggled, this, [this](bool) {
        if (m_loading)
            return;
        saveList();
        if (m_cycleTimer->isActive() && nextEnabledRow(-1) < 0) {
            m_cycleTimer->stop();
            m_cycle->setText(tr("循环发送"));
            m_listState->setText(tr("无使能行，循环已停"));
        }
    });

    auto *dirCombo = new QComboBox;
    dirCombo->addItem(tr("写"));
    dirCombo->addItem(tr("读"));
    dirCombo->blockSignals(true);
    dirCombo->setCurrentIndex(item.direction == DirRead ? 1 : 0);
    m_table->setCellWidget(row, ColDir, dirCombo);
    dirCombo->blockSignals(false);
    connect(dirCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_loading)
            saveList();
    });

    m_table->setItem(
        row, ColId,
        new QTableWidgetItem(QStringLiteral("0x%1").arg(linutil::formatHex2(
            static_cast<unsigned char>(item.id & 0x3F)))));

    auto *checkCombo = new QComboBox;
    checkCombo->addItem(tr("标准"));
    checkCombo->addItem(tr("增强"));
    checkCombo->blockSignals(true);
    checkCombo->setCurrentIndex(item.checkType == 1 ? 1 : 0);
    m_table->setCellWidget(row, ColCheck, checkCombo);
    checkCombo->blockSignals(false);
    connect(checkCombo, &QComboBox::currentIndexChanged, this,
            [this](int) {
                if (!m_loading)
                    saveList();
            });

    m_table->setItem(row, ColData, new QTableWidgetItem(item.dataHex));
    m_table->setItem(row, ColCycle,
                     new QTableWidgetItem(QString::number(item.cycleMs)));
}

int LinSendPanel::nextEnabledRow(int afterRow) const
{
    const int n = m_table->rowCount();
    if (n <= 0)
        return -1;

    auto enabled = [this](int r) {
        auto *en = qobject_cast<QCheckBox *>(m_table->cellWidget(r, ColEn));
        return en && en->isChecked();
    };

    for (int i = afterRow + 1; i < n; ++i) {
        if (enabled(i))
            return i;
    }
    if (afterRow >= 0) {
        for (int i = 0; i <= afterRow && i < n; ++i) {
            if (enabled(i))
                return i;
        }
    }
    return -1;
}
