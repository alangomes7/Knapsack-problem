#ifndef KNAPSACKWINDOW_H
#define KNAPSACKWINDOW_H

#include <QMainWindow>

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

private:
    Ui::knapsackWindow *ui;
};

#endif // KNAPSACKWINDOW_H
