#include "knapsackwindow.h"
#include "ui_knapsackwindow.h"

#include <memory>
#include <exception>
#include <chrono>

#include <QString>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QtConcurrent>
#include <QLineEdit>

#include "fileprocessor.h"
#include "algorithm.h"
#include "bag.h"

knapsackWindow::knapsackWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::knapsackWindow)
{
    ui->setupUi(this);
    initializeUi();
}

knapsackWindow::~knapsackWindow()
{
    delete ui;
}

void knapsackWindow::initializeUi()
{
    ui->pushButton_problem->setText("Select an instance");
    ui->timeEdit_maxExecutionTime->setDisplayFormat("mm:ss");
    ui->timeEdit_maxExecutionTime->setTime(QTime(0, 1, 30));
    ui->timeEdit_maxExecutionTime->setTimeRange(QTime(0, 0, 0, 0), QTime(0, 59, 59));
    ui->spinBox_algorithmSeed->setRange(0, 100);
    ui->spinBox_algorithmSeed->setValue(75);
    ui->spinBox_executionTimes->setRange(1, 100);
    ui->spinBox_executionTimes->setValue(5);
}

void knapsackWindow::on_pushButton_problem_clicked()
{
    try {
        QString problemFile = QFileDialog::getOpenFileName(
            this,
            tr("Open File"),
            "./../input/",
            tr("Text Files (*.txt);;Knapsack Files (*.knapsack)")
        );

        if (!problemFile.isEmpty()) {
            m_problemInstance = FileProcessor::loadProblem(problemFile.toStdString());
            QString problemPrint = QString::fromStdString(m_problemInstance.toString());
            ui->plainTextEdit_problem->setPlainText(problemPrint);
            ui->pushButton_problem->setText(problemFile);
        }
    } catch (const std::exception &e) {
        ui->pushButton_problem->setText("Select an instance");
        ui->plainTextEdit_problem->setPlainText("Error loading problem.");
        QMessageBox::critical(this, "Error", QString("Failed to load problem file:\n%1").arg(e.what()));
    } catch (...) {
        QMessageBox::critical(this, "Error", "An unknown error occurred.");
    }
}

void knapsackWindow::on_pushButton_findBag_clicked()
{
    // --- Disable UI elements ---
    ui->pushButton_findBag->setEnabled(false);
    ui->pushButton_problem->setEnabled(false);
    ui->pushButton_stop->setEnabled(true);
    m_stopRequested = false;

    // --- Parameters from UI ---
    QTime time = ui->timeEdit_maxExecutionTime->time();
    double maxExecutionTime = time.minute() * 60 + time.second();
    int seed = ui->spinBox_algorithmSeed->value();
    int maxExecutions = ui->spinBox_executionTimes->value();
    std::string timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss").toStdString();

    QFileInfo fileInfo(ui->pushButton_problem->text());
    QString folderPath = fileInfo.absolutePath();
    QString fileName = fileInfo.fileName();

    ProblemInstance problemCopy = m_problemInstance;

    auto start_time = std::chrono::steady_clock::now();
    std::chrono::milliseconds firstExecutionTime(0);

    // --- Background run (non-blocking) ---
    m_future = QtConcurrent::run([=, this]() mutable {

        // Helper: duration formatting
        auto formatDuration = [](long long totalMs) -> QString {
            long long hours = totalMs / 3600000;
            int minutes = (totalMs % 3600000) / 60000;
            int seconds = (totalMs % 60000) / 1000;
            int milliseconds = totalMs % 1000;
            return QStringLiteral("%1:%2:%3.%4")
                .arg(hours, 2, 10, QLatin1Char('0'))
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0'))
                .arg(milliseconds, 3, 10, QLatin1Char('0'));
        };

        auto resetUI = [=, this]() {
            QMetaObject::invokeMethod(this, [=]() {
                if (ui->pushButton_stop) {
                    ui->pushButton_stop->setText("Stop");
                    ui->pushButton_stop->setEnabled(true);
                }
                ui->pushButton_findBag->setEnabled(true);
                ui->pushButton_problem->setEnabled(true);
            }, Qt::QueuedConnection);
        };

        // --- Algorithm setup ---
        Algorithm algorithm(
            maxExecutionTime - 1,
            seed,
            folderPath.toStdString(),
            fileName.toStdString(),
            FileProcessor::formatTimestampForFilename(timestamp)
        );

        std::atomic<int> completed{0};

        // === Thread pool setup ===
        QThreadPool pool;
        int maxThreads = std::max(2u, std::thread::hardware_concurrency() / 2);
        pool.setMaxThreadCount(maxThreads);

        qDebug() << "Running" << maxExecutions << "executions with"
                 << pool.maxThreadCount() << "parallel threads.";

        // === Prepare tasks ===
        QList<int> tasks;
        for (int i = 0; i < maxExecutions; ++i)
            tasks.append(i);

        // === Parallel map ===
        QtConcurrent::blockingMap(&pool, tasks, [&](int execution) {
            if (m_stopRequested) return;

            auto exec_start = std::chrono::steady_clock::now();
            auto bagsExecution = algorithm.run(
                problemCopy,
                timestamp
            );
            auto exec_end = std::chrono::steady_clock::now();

            if (m_stopRequested) return;

            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                exec_end - exec_start
            );

            // Estimate total time based on first finished execution
            if (execution == 0) {
                firstExecutionTime = elapsedMs;
                auto estimatedTotalMs = firstExecutionTime.count() * maxExecutions;
                QMetaObject::invokeMethod(this, [=]() {
                    if (ui->timeEdit_estimatedTotalTime) {
                        ui->timeEdit_estimatedTotalTime->setDisplayFormat("hh:mm:ss.zzz");
                        QTime estimatedTime = QTime::fromMSecsSinceStartOfDay(
                            static_cast<int>(estimatedTotalMs % 86400000)
                        );
                        ui->timeEdit_estimatedTotalTime->setTime(estimatedTime);
                    }
                }, Qt::QueuedConnection);
            }

            // Progress bar update
            int progressValue = static_cast<int>(
                (100.0 * (++completed)) / maxExecutions
            );

            QMetaObject::invokeMethod(ui->progressBar, "setValue", Qt::QueuedConnection,
                                      Q_ARG(int, progressValue));

            // Update elapsed total time
            auto now = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_time
            ).count();

            QMetaObject::invokeMethod(this, [=]() {
                if (ui->timeEdit_executionTimeOverall) {
                    ui->timeEdit_executionTimeOverall->setDisplayFormat("hh:mm:ss.zzz");
                    QTime partialTime = QTime::fromMSecsSinceStartOfDay(
                        static_cast<int>(durationMs % 86400000)
                    );
                    ui->timeEdit_executionTimeOverall->setTime(partialTime);
                }
            }, Qt::QueuedConnection);
        });

        // === After all parallel executions ===
        if (m_stopRequested) {
            QMetaObject::invokeMethod(ui->progressBar, "setValue", Qt::QueuedConnection,
                                      Q_ARG(int, 0));
            qDebug() << "Execution stopped by user.";
            resetUI();
            return;
        }

        // Final timing
        auto end_time = std::chrono::steady_clock::now();
        auto totalDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        ).count();

        QString finalString = formatDuration(totalDurationMs);
        QMetaObject::invokeMethod(this, [=]() {
            if (ui->timeEdit_executionTimeOverall) {
                ui->timeEdit_executionTimeOverall->setDisplayFormat("hh:mm:ss.zzz");
                QTime finalTime = QTime::fromMSecsSinceStartOfDay(
                    static_cast<int>(totalDurationMs % 86400000)
                );
                ui->timeEdit_executionTimeOverall->setTime(finalTime);
            }
        }, Qt::QueuedConnection);

        QMetaObject::invokeMethod(ui->progressBar, "setValue", Qt::QueuedConnection,
                                  Q_ARG(int, 100));

        resetUI();
    });
}

void knapsackWindow::on_pushButton_stop_clicked()
{
    m_stopRequested = true;
    ui->pushButton_stop->setEnabled(false);
    ui->pushButton_stop->setText("Stopping...");
}

