#ifndef KNAPSACKWINDOW_H
#define KNAPSACKWINDOW_H

#include <QMainWindow>
#include <atomic>
#include <QFuture>
#include <QFutureWatcher>

#include "data_model.h"

QT_BEGIN_NAMESPACE
namespace Ui { class knapsackWindow; }
QT_END_NAMESPACE

class knapsackWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit knapsackWindow(QWidget *parent = nullptr);
    ~knapsackWindow();

private slots:
    void on_pushButton_problemFile_clicked();
    
    void on_pushButton_findBag_clicked();

    void on_pushButton_stop_clicked();

    void on_pushButton_reportFile_clicked();

    void on_pushButton_validateReport_clicked();

private:
    void initializeUi();

    Ui::knapsackWindow *ui;
    QFuture<void> m_future;
    std::atomic<bool> m_stopRequested{false};
    QFutureWatcher<void> m_watcher;

    ProblemInstance m_problemInstance;
};

#endif // KNAPSACKWINDOW_H
