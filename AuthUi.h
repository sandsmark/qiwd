#ifndef AUTHUI_H
#define AUTHUI_H

#include <QObject>
#include <QWidget>

class AuthUi : public QObject
{
    Q_OBJECT
public:
    explicit AuthUi(QObject *parent = nullptr);

signals:

};

#endif // AUTHUI_H
