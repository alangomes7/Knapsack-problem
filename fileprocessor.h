#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

#include <QObject>
#include <QString>
#include <QList>

#include <string>

#include "package.h"
#include "dependency.h"

class FileProcessor : public QObject
{
    Q_OBJECT
public:
    FileProcessor(QString filePath, QObject *parent);

    const QList<QList<QString>>& getProcessedData() const;

private:
    QList<QList<QString>> m_processedData;

    QString fileinputPath;
};

#endif // FILEPROCESSOR_H