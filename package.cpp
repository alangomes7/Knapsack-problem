#include "package.h"
#include "dependency.h"

Package::Package(const QString &name, int benefit, QObject *parent)
    : m_name(name),
      m_benefit(benefit),
      QObject(parent)
{
    m_dependencies = {};
}

const QList<Dependency*>& Package::getDependencies() const
{
    return m_dependencies;
}

int Package::getBenefit() const
{
    return m_benefit;
}

int Package::getSingleBenefit() const
{
    int dependenciesSize = 0;
    for(Dependency* dependency : m_dependencies){
        dependenciesSize += dependency->getSize();
    }
    return m_benefit - dependenciesSize;
}

void Package::addDependency(Dependency *dependency)
{
    m_dependencies.append(dependency);
}

QString Package::getName() const
{
    return m_name;
}

QString Package::toQString() const
{
    QString packageQString;
    packageQString += "Package: " + m_name + "\n";
    packageQString += "Benefit: " + QString::number(m_benefit) + " MB\n";
    packageQString += "Single benefit: " + QString::number(getSingleBenefit()) + " MB\n";

    for (const Dependency *dependency : m_dependencies) {
        packageQString += "Dependency: " + dependency->getName() + " | Size: " + QString::number(dependency->getSize()) + " MB\n";
    }

    return packageQString;
}
