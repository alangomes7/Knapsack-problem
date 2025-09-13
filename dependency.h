#ifndef DEPENDENCY_H
#define DEPENDENCY_H

#include <QObject>
#include <QString>
#include <QList>

class Package;

class Dependency : public QObject
{
    Q_OBJECT
public:

    explicit Dependency(const QString &name, int size, QObject *parent);

    QString getName() const;
    int getSize() const;
    int getMaxSize() const;

    const QList<Package*>& getAssociatedPackages() const;
    void addAssociatedPackage(Package* package);
    
    QString toQString() const;

private:
    QString m_name;
    int m_size;
    QList<Package*> m_associatedPackages;
    int m_totalBenefit;
};

#endif // DEPENDENCY_H