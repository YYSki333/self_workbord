#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

/** 侧边栏 LIN 运行配置（通道 / 波特率 / 主从 / 启停）。 */
class LinConfigPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LinConfigPanel(QWidget *parent = nullptr);

    void setChannelCount(int n);
    void setBusy(bool busy);
    void setRunning(bool running, const QString &stateText);
    int channel() const;
    int baudBps() const;
    bool isMaster() const;
    void setMaster(bool master);

signals:
    void startRequested();
    void stopRequested();

private:
    QComboBox *m_channel = nullptr;
    QComboBox *m_baud = nullptr;
    QComboBox *m_role = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_stop = nullptr;
    QLabel *m_state = nullptr;
};
