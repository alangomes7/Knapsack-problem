#ifndef PACKAGE_H
#define PACKAGE_H

#include <QObject>
#include <QString>
#include <QList>

class Dependency;

class Package : public QObject
{
    Q_OBJECT
public:

    explicit Package(const QString &name, int benefit, QObject *parent);

    const QList<Dependency*>& getDependencies() const;

    int getBenefit() const;
    int getSingleBenefit() const;
    QString getName() const;
    QString toQString() const;

    void addDependency(Dependency *dependency);

private:
    QString m_name;
    int m_benefit;

    QList<Dependency*> m_dependencies;
};

#endif // PACKAGE_H