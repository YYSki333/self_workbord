#include "ui/devicepanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

DevicePanel::DevicePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(6);

    auto *title = new QLabel(tr("设备"));
    title->setObjectName(QStringLiteral("sideTitle"));
    title->setStyleSheet(QStringLiteral("font-weight:600;padding:4px 0;"));
    root->addWidget(title);

    auto *row = new QHBoxLayout;
    m_scan = new QPushButton(tr("扫描设备"));
    m_close = new QPushButton(tr("关闭"));
    m_close->setEnabled(false);
    row->addWidget(m_scan, 1);
    row->addWidget(m_close);
    root->addLayout(row);

    m_info = new QLabel(tr("未扫描"));
    m_info->setWordWrap(true);
    root->addWidget(m_info);

    m_combo = new QComboBox;
    m_combo->setPlaceholderText(tr("选择设备…"));
    root->addWidget(m_combo);

    connect(m_scan, &QPushButton::clicked, this, &DevicePanel::scanRequested);
    connect(m_close, &QPushButton::clicked, this, &DevicePanel::closeRequested);
    connect(m_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        emit deviceSelected(currentHandle());
    });
}

void DevicePanel::setBusy(bool busy)
{
    m_scan->setEnabled(!busy);
    m_close->setEnabled(!busy && !m_devices.isEmpty());
    m_combo->setEnabled(!busy);
}

void DevicePanel::clearDevices()
{
    m_devices.clear();
    m_combo->clear();
    m_close->setEnabled(false);
    m_info->setText(tr("已关闭"));
}

void DevicePanel::setDevices(const QVector<DeviceEntry> &devices)
{
    m_devices = devices;
    m_combo->clear();
    for (const DeviceEntry &e : m_devices)
        m_combo->addItem(e.label, e.handle);
    m_close->setEnabled(!m_devices.isEmpty());
}

int DevicePanel::currentHandle() const
{
    return m_combo->currentData().toInt();
}

bool DevicePanel::hasDevice() const
{
    return !m_devices.isEmpty();
}

void DevicePanel::setInfoText(const QString &text)
{
    m_info->setText(text);
}
