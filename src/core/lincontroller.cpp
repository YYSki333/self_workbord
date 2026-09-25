#include "core/lincontroller.h"

#include "core/linutil.h"
#include "usb_device.h"

#include <QDateTime>
#include <QTimerEvent>

namespace {
constexpr int kPollMs = 50;
}

LinController::LinController(QObject *parent)
    : QObject(parent)
{
}

LinController::~LinController()
{
    stop();
}

int LinController::start(int handle, int channel, int baudBps, bool master)
{
    if (m_running)
        return LIN_EX_ERR_CMD_FAIL;
    if (handle < 0) {
        emit errorOccurred(tr("无效设备句柄，未启动 LIN"));
        return LIN_EX_ERR_PARAMETER;
    }
    if (channel < 0 || channel > 3 || baudBps < 2000 || baudBps > 100000) {
        emit errorOccurred(tr("LIN 参数非法（通道/波特率）"));
        return LIN_EX_ERR_PARAMETER;
    }

    const int ret =
        LIN_EX_Init(handle, static_cast<unsigned char>(channel),
                    static_cast<unsigned int>(baudBps),
                    master ? LIN_EX_MASTER : LIN_EX_SLAVE);
    if (ret != LIN_EX_SUCCESS) {
        emit errorOccurred(tr("初始化 LIN%1 失败（%2）")
                               .arg(channel + 1)
                               .arg(ret));
        return ret;
    }

    m_handle = handle;
    m_channel = channel;
    m_master = master;
    m_running = true;
    m_timerId = startTimer(kPollMs, Qt::PreciseTimer);
    emit started(channel, master, baudBps);
    return LIN_EX_SUCCESS;
}

void LinController::stop()
{
    if (m_timerId) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    if (m_running && m_handle >= 0)
        USB_ResetDevice(m_handle);

    const bool was = m_running;
    m_running = false;
    m_handle = -1;
    m_channel = 0;
    if (was)
        emit stopped();
}

int LinController::write(int handle, int channel, unsigned char id,
                         const unsigned char *data, int len,
                         unsigned char checkType)
{
    if (!m_running || handle < 0 || !data || len < 0 || len > 8)
        return LIN_EX_ERR_PARAMETER;
    if (channel != m_channel || handle != m_handle)
        return LIN_EX_ERR_PARAMETER;

    const unsigned char pid = calcPid(id);
    // 写期间暂停轮询，避免与 MasterWrite 抢 USB
    const bool resumePoll = m_timerId != 0;
    if (resumePoll) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    const int ret = LIN_EX_MasterWrite(
        handle, static_cast<unsigned char>(channel), pid,
        const_cast<unsigned char *>(data),
        static_cast<unsigned char>(len), checkType);
    if (resumePoll && m_running)
        m_timerId = startTimer(kPollMs, Qt::PreciseTimer);
    return ret;
}

int LinController::read(int handle, int channel, unsigned char id,
                        unsigned char *outData, int maxLen)
{
    if (!m_running || handle < 0 || !outData || maxLen <= 0)
        return LIN_EX_ERR_PARAMETER;
    if (channel != m_channel || handle != m_handle)
        return LIN_EX_ERR_PARAMETER;

    const unsigned char pid = calcPid(id);
    unsigned char buf[8] = {};
    const bool resumePoll = m_timerId != 0;
    if (resumePoll) {
        killTimer(m_timerId);
        m_timerId = 0;
    }
    const int n = LIN_EX_MasterRead(handle,
                                    static_cast<unsigned char>(channel),
                                    pid, buf);
    if (resumePoll && m_running)
        m_timerId = startTimer(kPollMs, Qt::PreciseTimer);
    if (n > 0) {
        const int c = qMin(n, maxLen);
        for (int i = 0; i < c; ++i)
            outData[i] = buf[i];
    }
    return n;
}

void LinController::timerEvent(QTimerEvent *event)
{
    if (event->timerId() != m_timerId || !m_running)
        return;
    poll();
}

void LinController::poll()
{
    if (!m_running || m_handle < 0)
        return;

    LIN_EX_MSG msgs[128];
    TraceFrameList out;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    int n = 0;
    if (!m_master) {
        n = LIN_EX_SlaveGetData(m_handle,
                                static_cast<unsigned char>(m_channel), msgs);
        if (n < 0)
            n = 0;
        for (int i = 0; i < n; ++i)
            out.append(toFrame(msgs[i], m_channel, now, false));
    } else {
        n = LIN_EX_GetMsg(m_handle, static_cast<unsigned char>(m_channel),
                          msgs, 128);
        if (n < 0)
            n = 0;
        for (int i = 0; i < n; ++i) {
            const bool tx = msgs[i].MsgType == LIN_EX_MSG_TYPE_MW
                            || msgs[i].MsgType == LIN_EX_MSG_TYPE_SW;
            out.append(toFrame(msgs[i], m_channel, now, tx));
        }
    }

    if (!out.isEmpty())
        emit framesCaptured(out);
}

unsigned char LinController::calcPid(unsigned char id)
{
    return linutil::calcPid(id);
}

static QString typeName(unsigned char t)
{
    switch (t) {
    case LIN_EX_MSG_TYPE_MW: return QObject::tr("主机写 MW");
    case LIN_EX_MSG_TYPE_MR: return QObject::tr("主机读 MR");
    case LIN_EX_MSG_TYPE_SW: return QObject::tr("从机写 SW");
    case LIN_EX_MSG_TYPE_SR: return QObject::tr("从机读 SR");
    case LIN_EX_MSG_TYPE_BK: return QObject::tr("BREAK");
    case LIN_EX_MSG_TYPE_SY: return QObject::tr("SYNC");
    case LIN_EX_MSG_TYPE_ID: return QObject::tr("PID");
    case LIN_EX_MSG_TYPE_DT: return QObject::tr("DATA");
    case LIN_EX_MSG_TYPE_CK: return QObject::tr("CHECK");
    default: return QObject::tr("未知");
    }
}

static QString checkName(unsigned char t)
{
    switch (t) {
    case LIN_EX_CHECK_EXT: return QObject::tr("增强");
    case LIN_EX_CHECK_NONE: return QObject::tr("无");
    case LIN_EX_CHECK_ERROR: return QObject::tr("错误");
    default: return QObject::tr("标准");
    }
}

TraceFrame LinController::toFrame(const LIN_EX_MSG &msg, int channel,
                                  qint64 epochMs, bool isTx)
{
    TraceFrame f;
    f.epochMs = epochMs;
    f.isTx = isTx;
    f.channel = channel;
    f.bus = QStringLiteral("LIN");
    f.direction = isTx ? QStringLiteral("TX") : QStringLiteral("RX");
    f.type = typeName(msg.MsgType);
    f.idText = linutil::formatHex2(static_cast<unsigned char>(msg.PID & 0x3F));
    f.pidText = linutil::formatHex2(msg.PID);
    f.dlcText = QString::number(msg.DataLen);

    const int dlen = msg.DataLen > 8 ? 8 : msg.DataLen;
    f.dataText = linutil::formatBytes(msg.Data, dlen);
    f.checkText = QStringLiteral("%1 %2")
                      .arg(linutil::formatHex2(msg.Check))
                      .arg(checkName(msg.CheckType));
    return f;
}
