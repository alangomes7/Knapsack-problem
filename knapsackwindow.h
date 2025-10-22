#ifndef KNAPSACKWINDOW_H
#define KNAPSACKWINDOW_H

#include <QMainWindow>

#include "DataStructures.h"

namespace Ui {
class knapsackWindow;
}

class knapsackWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit knapsackWindow(QWidget *parent = nullptr);
    ~knapsackWindow();

private slots:
    void on_pushButton_problem_clicked();

    void on_pushButton_findBag_clicked();

private:
    void initializeUi();
    Ui::knapsackWindow *ui;
    ProblemInstance m_problemInstance;
};

#endif // KNAPSACKWINDOW_H
