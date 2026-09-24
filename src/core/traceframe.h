#pragma once

#include <QVector>
#include <QString>

/** 统一报文帧：各协议写入 Trace 前先转成该结构。 */
struct TraceFrame {
    qint64 epochMs = 0;  // 系统时间 ms since epoch
    bool isTx = false;
    int channel = 0;
    QString direction;
    QString type;
    QString idText;
    QString pidText;
    QString dlcText;
    QString dataText;
    QString checkText;    // checksum hex + 类型
    QString bus;
};

using TraceFrameList = QVector<TraceFrame>;
