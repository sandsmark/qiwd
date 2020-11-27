#ifndef ADAPTER_H
#define ADAPTER_H

#include <QDBusObjectPath>
#include <QMap>
#include <QList>
#include <QString>
#include <QVariant>

using ManagedObject = QMap<QString,QVariantMap>;
Q_DECLARE_METATYPE(ManagedObject)
using ManagedObjectList = QMap<QDBusObjectPath,ManagedObject>;
Q_DECLARE_METATYPE(ManagedObjectList);

using OrderedNetworkList = QList<QPair<QDBusObjectPath,int16_t>>;
Q_DECLARE_METATYPE(OrderedNetworkList);

using LevelsList = QList<int16_t>;
Q_DECLARE_METATYPE(LevelsList);

#endif // ADAPTER_H
