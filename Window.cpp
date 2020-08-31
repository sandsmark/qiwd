#include "Window.h"

#include "AuthUi.h"

#include <QDBusConnection>
#include <QVBoxLayout>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>

void Window::onKnownNetworkRemoved(const QString &networkId)
{
    const QString name = m_iwd.networkName(networkId);
    if (name.isEmpty()) {
        qWarning() << "No name for" << networkId;
        return;
    }

    for (const QListWidgetItem *item : m_knownNetworksList->findItems(name, Qt::MatchExactly)) {
        delete m_knownNetworksList->takeItem(m_knownNetworksList->row(item));
    }
}

void Window::onKnownNetworkAdded(const QString &networkId, const QString &name)
{
    qDebug() << "known network" << networkId << name;
    Q_UNUSED(networkId);
    m_knownNetworksList->addItem(name);
}

void Window::onDeviceAdded(const QString &stationId, const QString &name)
{
    m_deviceList->addItem(name, stationId);
}

void Window::onDeviceRemoved(const QString &stationId)
{
    for (int i=0; i<m_deviceList->count(); i++) {
        if (m_deviceList->itemData(i) == stationId) {
            m_deviceList->removeItem(i);
        }
    }
}

void Window::onDisconnectDevice()
{
    m_iwd.disconnectStation(m_deviceList->currentData().toString());
}

void Window::onVisibleNetworkRemoved(const QString &name)
{
    for (const QListWidgetItem *item : m_networkList->findItems(name, Qt::MatchExactly)) {
        delete m_networkList->takeItem(m_networkList->row(item));
    }
}

void Window::onVisibleNetworkAdded(const QString &name)
{
    m_networkList->addItem(name);
}

Window::Window(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    m_networkList = new QListWidget;
    m_knownNetworksList = new QListWidget;

    QHBoxLayout *devicesLayout = new QHBoxLayout;
    m_deviceList = new QComboBox;
    QPushButton *disconnectButton = new QPushButton(tr("Disconnect"));

    devicesLayout->addStretch();
    devicesLayout->addWidget(m_deviceList);
    devicesLayout->addWidget(disconnectButton);

    mainLayout->addLayout(devicesLayout);
    mainLayout->addWidget(m_networkList);
    mainLayout->addWidget(m_knownNetworksList);

    connect(disconnectButton, &QPushButton::clicked, this, &Window::onDisconnectDevice);

    connect(&m_iwd, &Iwd::visibleNetworkAdded, this, &Window::onVisibleNetworkAdded);
    connect(&m_iwd, &Iwd::visibleNetworkRemoved, this, &Window::onVisibleNetworkRemoved);
    connect(&m_iwd, &Iwd::knownNetworkAdded, this, &Window::onKnownNetworkAdded);
    connect(&m_iwd, &Iwd::knownNetworkRemoved, this, &Window::onKnownNetworkRemoved);
    connect(&m_iwd, &Iwd::deviceAdded, this, &Window::onDeviceAdded);
    connect(&m_iwd, &Iwd::deviceRemoved, this, &Window::onDeviceRemoved);

    m_signalAgent = new SignalLevelAgent(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_signalAgent->objectPath().path(), this)) {
        m_iwd.setSignalAgent(m_signalAgent->objectPath(), {-20, -40, -49, -50, -51, -60, -80});
        qDebug() << "set signal agent";
    } else {
        qWarning() << "Failed to register signal agent";
    }
    connect(m_signalAgent, &SignalLevelAgent::signalLevelChanged, this, [=](const QString &stationId, int newLevel) {
        qDebug() << "Signal level for" << stationId << "is now" << newLevel;
    });

    m_authUi = new AuthUi(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_authUi->objectPath().path(), this)) {
        m_iwd.setAuthAgent(m_authUi->objectPath());
    } else {
        qWarning() << "Failed to register auth agent";
    }

    m_iwd.init();
}
