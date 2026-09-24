#include "ui/tracepanel.h"

#include "ui/uifont.h"

#include <QCheckBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
constexpr int kInitialCapacity = 5000;
constexpr int kHardMaxCapacity = 200000;
const char *kCols[] = {
    "时间", "方向", "类型", "总线", "通道",
    "ID", "PID", "DLC", "Data", "Checksum"};
}

TracePanel::TracePanel(QWidget *parent)
    : QWidget(parent)
    , m_capacity(kInitialCapacity)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *bar = new QHBoxLayout;
    auto *title = new QLabel(tr("报文 Trace"));
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    bar->addWidget(title);
    bar->addStretch();

    m_autoScroll = new QCheckBox(tr("自动滚动"));
    m_autoScroll->setChecked(true);
    m_showTx = new QCheckBox(tr("显示发送"));
    m_showTx->setChecked(true);
    m_showRx = new QCheckBox(tr("显示接收"));
    m_showRx->setChecked(true);
    m_clear = new QPushButton(tr("清空"));
    m_count = new QLabel;

    bar->addWidget(m_autoScroll);
    bar->addWidget(m_showTx);
    bar->addWidget(m_showRx);
    bar->addWidget(m_clear);
    bar->addWidget(m_count);
    root->addLayout(bar);

    m_table = new QTableWidget;
    // Trace 用等宽：时间/ID/Data 对齐更易读
    uifont::applyMono(m_table, 10);
    m_table->setColumnCount(int(sizeof(kCols) / sizeof(kCols[0])));
    QStringList headers;
    for (const char *c : kCols)
        headers << QString::fromUtf8(c);
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);

    auto *hdr = m_table->horizontalHeader();
    hdr->setSectionResizeMode(QHeaderView::Interactive);
    hdr->setStretchLastSection(true);
    hdr->setMinimumSectionSize(48);
    const int widths[] = {100, 48, 88, 44, 52, 56, 56, 44, 160, 88};
    for (int i = 0; i < m_table->columnCount()
                    && i < int(sizeof(widths) / sizeof(int)); ++i)
        m_table->setColumnWidth(i, widths[i]);

    root->addWidget(m_table, 1);
    updateCountLabel();

    connect(m_clear, &QPushButton::clicked, this, &TracePanel::clear);
    connect(m_showTx, &QCheckBox::toggled, this, &TracePanel::rebuild);
    connect(m_showRx, &QCheckBox::toggled, this, &TracePanel::rebuild);
}

void TracePanel::appendFrames(const TraceFrameList &frames)
{
    if (frames.isEmpty())
        return;

    for (const TraceFrame &f : frames) {
        m_rows.append(f);
        ++m_total;
        ensureCapacity();
        if (!rowVisible(f))
            continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        insertRow(r, f);
    }

    trimToCapacity();

    // 表格行可能因裁剪与缓存不一致：批量追加后校正一次
    if (m_table->rowCount() > m_rows.size())
        rebuild();

    updateCountLabel();
    emit frameCountChanged(m_total);

    if (m_autoScroll->isChecked() && m_table->rowCount() > 0)
        m_table->scrollToBottom();
}

void TracePanel::clear()
{
    m_rows.clear();
    m_total = 0;
    m_capacity = kInitialCapacity;
    m_table->setRowCount(0);
    updateCountLabel();
    emit frameCountChanged(0);
    emit capacityChanged(m_capacity);
}

void TracePanel::rebuild()
{
    m_table->setRowCount(0);
    for (const TraceFrame &f : m_rows) {
        if (!rowVisible(f))
            continue;
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        insertRow(r, f);
    }
    updateCountLabel();
    emit frameCountChanged(m_total);
}

void TracePanel::ensureCapacity()
{
    if (m_rows.size() < m_capacity)
        return;
    if (m_capacity >= kHardMaxCapacity)
        return;
    m_capacity = qMin(m_capacity * 2, kHardMaxCapacity);
    emit capacityChanged(m_capacity);
}

void TracePanel::trimToCapacity()
{
    if (m_rows.size() <= m_capacity)
        return;
    const int drop = m_rows.size() - m_capacity;
    m_rows.remove(0, drop);
    // 表格同步：从顶部删行（若可见行包含被删帧会不准，统一 rebuild 更稳）
    if (m_table->rowCount() > 0)
        rebuild();
}

void TracePanel::updateCountLabel()
{
    m_count->setText(tr("%1 帧 · 缓存上限 %2")
                         .arg(m_total)
                         .arg(m_capacity));
}

void TracePanel::insertRow(int tableRow, const TraceFrame &f)
{
    auto set = [&](int col, const QString &s) {
        m_table->setItem(tableRow, col, new QTableWidgetItem(s));
    };

    const QDateTime dt = QDateTime::fromMSecsSinceEpoch(f.epochMs);
    set(0, dt.toString(QStringLiteral("HH:mm:ss.zzz")));
    set(1, f.direction);
    set(2, f.type);
    set(3, f.bus);
    set(4, QStringLiteral("%1%2").arg(f.bus).arg(f.channel + 1));
    set(5, f.idText);
    set(6, f.pidText);
    set(7, f.dlcText);
    set(8, f.dataText);
    set(9, f.checkText);
}

bool TracePanel::rowVisible(const TraceFrame &f) const
{
    if (f.isTx)
        return m_showTx->isChecked();
    return m_showRx->isChecked();
}
