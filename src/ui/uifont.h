#pragma once

#include <QFont>
#include <QFontDatabase>
#include <QStringList>
#include <QWidget>

namespace uifont {

/** 等宽字体（优先常见控制台/代码字体）。 */
inline QFont monoFont(int pointSize = -1)
{
    static const QStringList preferred = {
        QStringLiteral("Cascadia Mono"),
        QStringLiteral("Consolas"),
        QStringLiteral("JetBrains Mono"),
        QStringLiteral("Source Code Pro"),
        QStringLiteral("DejaVu Sans Mono"),
        QStringLiteral("Liberation Mono"),
        QStringLiteral("Courier New"),
    };

    const QStringList available =
        QFontDatabase::families();
    QString family = QStringLiteral("monospace");
    for (const QString &p : preferred) {
        if (available.contains(p, Qt::CaseInsensitive)) {
            family = p;
            break;
        }
    }

    QFont f(family);
    if (pointSize > 0)
        f.setPointSize(pointSize);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return f;
}

/** 给表格/输入框套等宽。 */
inline void applyMono(QWidget *w, int pointSize = -1)
{
    if (!w)
        return;
    w->setFont(monoFont(pointSize));
}

} // namespace uifont
