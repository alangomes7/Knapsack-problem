#ifndef BAG_H
#define BAG_H

#include <QObject>
#include "package.h"

class Bag : public QObject
{
    Q_OBJECT
public:
    Bag(float capacity, QObject *parent);

signals:

private:
    float m_capacity;
    Package* baged_packages;
};
#endif // BAG_H
