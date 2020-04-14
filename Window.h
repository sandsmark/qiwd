#ifndef WINDOW_H
#define WINDOW_H

#include <QWidget>
#include <QPointer>

#include "Iwd.h"

class QListWidget;
class QComboBox;
class AuthUi;
class SignalLevelAgent;

class Window : public QWidget
{
    Q_OBJECT
public:
    explicit Window(QWidget *parent = nullptr);

private:
    QListWidget *m_knownNetworksList = nullptr;
    QListWidget *m_networkList = nullptr;
    QComboBox *m_deviceList = nullptr;

    Iwd m_iwd;
    QPointer<SignalLevelAgent> m_signalAgent;
};

#endif // WINDOW_H
