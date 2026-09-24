#include "ui/linconfigpanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

LinConfigPanel::LinConfigPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    auto *title = new QLabel(tr("LIN 配置"));
    title->setStyleSheet(QStringLiteral("font-weight:600;padding:4px 0;"));
    root->addWidget(title);

    m_channel = new QComboBox;
    m_channel->addItems({tr("LIN1"), tr("LIN2")});
    root->addWidget(m_channel);

    m_baud = new QComboBox;
    m_baud->addItems({QStringLiteral("19200"), QStringLiteral("10400"),
                      QStringLiteral("9600"), QStringLiteral("4800"),
                      QStringLiteral("2400"), QStringLiteral("2000")});
    root->addWidget(m_baud);

    m_role = new QComboBox;
    m_role->addItems({tr("主机"), tr("从机")});
    root->addWidget(m_role);

    auto *row = new QHBoxLayout;
    m_start = new QPushButton(tr("启动"));
    m_stop = new QPushButton(tr("停止"));
    m_stop->setEnabled(false);
    row->addWidget(m_start, 1);
    row->addWidget(m_stop);
    root->addLayout(row);

    m_state = new QLabel(tr("未启动"));
    m_state->setWordWrap(true);
    root->addWidget(m_state);
    root->addStretch();

    connect(m_start, &QPushButton::clicked, this,
            &LinConfigPanel::startRequested);
    connect(m_stop, &QPushButton::clicked, this,
            &LinConfigPanel::stopRequested);
}

void LinConfigPanel::setChannelCount(int n)
{
    const int old = m_channel->currentIndex();
    m_channel->clear();
    for (int i = 0; i < qMax(1, n); ++i)
        m_channel->addItem(QStringLiteral("LIN%1").arg(i + 1));
    if (old >= 0 && old < m_channel->count())
        m_channel->setCurrentIndex(old);
}

void LinConfigPanel::setBusy(bool busy)
{
    m_channel->setEnabled(!busy);
    m_baud->setEnabled(!busy);
    m_role->setEnabled(!busy);
    m_start->setEnabled(!busy);
}

void LinConfigPanel::setRunning(bool running, const QString &stateText)
{
    m_start->setEnabled(!running);
    m_stop->setEnabled(running);
    m_channel->setEnabled(!running);
    m_baud->setEnabled(!running);
    m_role->setEnabled(!running);
    m_state->setText(stateText);
}

int LinConfigPanel::channel() const
{
    return m_channel->currentIndex();
}

int LinConfigPanel::baudBps() const
{
    return m_baud->currentText().toInt();
}

bool LinConfigPanel::isMaster() const
{
    return m_role->currentIndex() == 0;
}

void LinConfigPanel::setMaster(bool master)
{
    m_role->setCurrentIndex(master ? 0 : 1);
}
