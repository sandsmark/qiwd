#include "Window.h"

#include "AuthUi.h"

#include <QDBusConnection>
#include <QVBoxLayout>
#include <QComboBox>
#include <QListWidget>
#include <QListWidgetItem>

Window::Window(QWidget *parent) : QWidget(parent)
{
    setLayout(new QVBoxLayout);
    m_deviceList = new QComboBox;
    m_networkList = new QListWidget;
    m_knownNetworksList = new QListWidget;

    layout()->addWidget(m_deviceList);
    layout()->addWidget(m_networkList);
    layout()->addWidget(m_knownNetworksList);

    connect(&m_iwd, &Iwd::visibleNetworkAdded, this, [=](const QString &name){
        m_networkList->addItem(name);
    });
    connect(&m_iwd, &Iwd::visibleNetworkRemoved, this, [=](const QString &name){
        for (const QListWidgetItem *item : m_networkList->findItems(name, Qt::MatchExactly)) {
            delete m_networkList->takeItem(m_networkList->row(item));
        }
    });

    connect(&m_iwd, &Iwd::knownNetworkAdded, this, [=](const QString &name, const QString &id){
        Q_UNUSED(id);
        m_knownNetworksList->addItem(name);
    });
    connect(&m_iwd, &Iwd::knownNetworkRemoved, this, [=](const QString &name){
        for (const QListWidgetItem *item : m_knownNetworksList->findItems(name, Qt::MatchExactly)) {
            delete m_knownNetworksList->takeItem(m_knownNetworksList->row(item));
        }
    });

    connect(&m_iwd, &Iwd::deviceAdded, this, [=](const QString &name){
        m_deviceList->addItem(name);
    });
    connect(&m_iwd, &Iwd::deviceRemoved, this, [=](const QString &name){
        for (int i=0; i<m_deviceList->count(); i++) {
            if (m_deviceList->itemText(i) == name) {
                m_deviceList->removeItem(i);
                return;
            }
        }
    });
    m_signalAgent = new SignalLevelAgent(&m_iwd);
    if (QDBusConnection::systemBus().registerObject(m_signalAgent->objectPath().path(), this)) {
        m_iwd.setSignalAgent(m_signalAgent->objectPath(), {-20, -40, -49, -50, -51, -60, -80});
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
