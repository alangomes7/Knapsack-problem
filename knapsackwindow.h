#ifndef KNAPSACKWINDOW_H
#define KNAPSACKWINDOW_H

#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <QMainWindow>
#include <QString>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui {
class KnapsackWindow;
}
QT_END_NAMESPACE

class KnapsackWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit KnapsackWindow(QWidget *parent = nullptr);
    ~KnapsackWindow();

private:
    void initializeUiElements();
    void connections();
    void pushButton_filePath_clicked();
    void pushButton_run_clicked();
    void loadFile();
    void printPackages();
    void printDependecies();

    Ui::KnapsackWindow *ui;
    Bag* bag;
    QList<Package*> m_unsacked_packages;
    QList<Dependency*> m_available_depencies;
    int bag_size;

    QString m_filePath;
};
#endif // KNAPSACKWINDOW_H