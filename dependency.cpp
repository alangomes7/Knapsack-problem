#include "dependency.h"
#include "package.h"

Dependency::Dependency(const QString &name, int size, QObject *parent)
    : m_name(name),
      m_size(size),
      QObject(parent)
{
    m_totalBenefit = 0;
}

QString Dependency::getName() const
{
    return m_name;
}

int Dependency::getSize() const
{
    return m_size;
}

int Dependency::getMaxSize() const
{
    return m_size - m_totalBenefit;
}

const QList<Package *> &Dependency::getAssociatedPackages() const
{
    return m_associatedPackages;
}

void Dependency::addAssociatedPackage(Package *package)
{
    m_associatedPackages.append(package);
    m_totalBenefit += package->getBenefit();
}

QString Dependency::toQString() const
{
    return QString::number(m_size);
}
