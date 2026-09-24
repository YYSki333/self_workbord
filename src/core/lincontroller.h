#pragma once

#include "core/traceframe.h"

#include <QObject>

#include "usb2lin_ex.h"

/** LIN 协议控制器：初始化、轮询收包、主机写/读。 */
class LinController : public QObject
{
    Q_OBJECT
public:
    explicit LinController(QObject *parent = nullptr);
    ~LinController() override;

    bool isRunning() const { return m_running; }

    int start(int handle, int channel, int baudBps, bool master);
    void stop();

    int write(int handle, int channel, unsigned char id,
              const unsigned char *data, int len, unsigned char checkType);
    int read(int handle, int channel, unsigned char id,
             unsigned char *outData, int maxLen);

signals:
    void framesCaptured(const TraceFrameList &frames);
    void started(int channel, bool master, int baudBps);
    void stopped();
    void errorOccurred(const QString &message);

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    static unsigned char calcPid(unsigned char id);
    static TraceFrame toFrame(const LIN_EX_MSG &msg, int channel,
                              qint64 epochMs, bool isTx);
    void poll();

    int m_timerId = 0;
    bool m_running = false;
    bool m_master = true;
    int m_handle = -1;
    int m_channel = 0;
};
