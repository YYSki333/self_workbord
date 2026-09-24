#pragma once

#include "core/traceframe.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QLabel;
class QTimer;

/**
 * LIN 发送面板：
 * - 单条：ID / 校验 / Data → 写、读
 * - 列表：每行可配置方向（主机写 / 主机读），支持发送一遍与循环
 * - 列表经 QSettings 持久化
 */
class LinSendPanel : public QWidget
{
    Q_OBJECT
public:
    enum Direction {
        DirWrite = 0, ///< 主机写 MW
        DirRead = 1,  ///< 主机读 MR
    };

    struct ListItem {
        bool enabled = true;
        int direction = DirWrite;
        int id = 1;
        unsigned char checkType = 0; // 0 标准 1 增强
        QString dataHex;
        int cycleMs = 100;
    };

    explicit LinSendPanel(QWidget *parent = nullptr);
    ~LinSendPanel() override;

    void setEnabledForRun(bool running, bool master);

    int sendId() const;
    unsigned char checkType() const;
    QString dataHex() const;

    QList<ListItem> enabledItems() const;
    bool isCycleRunning() const;

signals:
    void writeRequested();
    void readRequested();
    /** 列表一行：按 direction 执行写或读。 */
    void listItemRequested(const ListItem &item);
    void cycleFinished();
    void statusText(const QString &text);

private slots:
    void onAddRow();
    void onRemoveRows();
    void onSendOnce();
    void onToggleCycle();
    void onCycleTick();

private:
    void buildUi();
    void loadList();
    void saveList() const;
    void updateListUi();
    ListItem rowToItem(int row) const;
    void applyRow(int row, const ListItem &item);
    int nextEnabledRow(int afterRow) const;
    void emitItem(const ListItem &item);

    QSpinBox *m_id = nullptr;
    QComboBox *m_check = nullptr;
    QLineEdit *m_data = nullptr;
    QPushButton *m_write = nullptr;
    QPushButton *m_read = nullptr;
    QComboBox *m_defaultDir = nullptr;

    QTableWidget *m_table = nullptr;
    QPushButton *m_add = nullptr;
    QPushButton *m_del = nullptr;
    QPushButton *m_sendOnce = nullptr;
    QPushButton *m_cycle = nullptr;
    QSpinBox *m_defaultCycle = nullptr;
    QLabel *m_listState = nullptr;
    QTimer *m_cycleTimer = nullptr;
    int m_cycleRow = -1;
    bool m_running = false;
    bool m_master = true;
    bool m_loading = false;
};
