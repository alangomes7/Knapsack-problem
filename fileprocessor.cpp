#include "FileProcessor.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

FileProcessor::FileProcessor(QString filePath, QObject *parent)
    : fileinputPath (filePath), QObject(parent)
{
    QFile fileinput(fileinputPath);

    // Try to open the file in read-only text mode.
    if (!fileinput.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // If the file cannot be opened, print a warning to the debug output.
        qWarning() << "Error: Could not open file" << fileinputPath;
        return; // Stop processing.
    }

    // Use QTextStream for convenient line-by-line reading from a QFile.
    QTextStream in(&fileinput);

    // Read the file until the end is reached.
    while (!in.atEnd()) {
        QString line = in.readLine();

        // Split the line into a list of words.
        // We use a regular expression to split by one or more whitespace characters (\s+),
        // which correctly handles spaces, tabs, and multiple spaces between words.
        // Qt::SkipEmptyParts ensures that we don't get empty strings in our list.
        QList<QString> words = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        // Add the list of words for the current line to our main data structure.
        // You could add a check 'if (!words.isEmpty())' here if you want to ignore empty lines.
        m_processedData.append(words);
    }

    // The file is closed automatically when the 'file' object goes out of scope.
}

const QList<QList<QString>> &FileProcessor::getProcessedData() const
{
    return m_processedData;
}
