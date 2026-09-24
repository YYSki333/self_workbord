#pragma once

#include "core/traceframe.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

/** 共享报文 Trace：过滤、清空；容量触顶自动翻倍（上限可配）。 */
class TracePanel : public QWidget
{
    Q_OBJECT
public:
    explicit TracePanel(QWidget *parent = nullptr);

    void appendFrames(const TraceFrameList &frames);
    void clear();
    int frameCount() const { return m_total; }
    int capacity() const { return m_capacity; }

signals:
    void frameCountChanged(int total);
    void capacityChanged(int capacity);

private slots:
    void rebuild();

private:
    void ensureCapacity();
    void trimToCapacity();
    void updateCountLabel();
    void insertRow(int tableRow, const TraceFrame &f);
    bool rowVisible(const TraceFrame &f) const;

    QTableWidget *m_table = nullptr;
    QCheckBox *m_autoScroll = nullptr;
    QCheckBox *m_showTx = nullptr;
    QCheckBox *m_showRx = nullptr;
    QPushButton *m_clear = nullptr;
    QLabel *m_count = nullptr;
    TraceFrameList m_rows;
    int m_total = 0;
    int m_capacity = 5000;
};
