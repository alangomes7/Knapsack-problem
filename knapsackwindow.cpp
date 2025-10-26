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

#include "file_processor.h"
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
    ui->pushButton_problemFile->setText("Select an instance");
    ui->timeEdit_maxExecutionTime->setDisplayFormat("mm:ss");
    ui->timeEdit_maxExecutionTime->setTime(QTime(0, 1, 30));
    ui->timeEdit_maxExecutionTime->setTimeRange(QTime(0, 0, 0, 0), QTime(0, 59, 59));
    ui->spinBox_algorithmSeed->setRange(0, 100);
    ui->spinBox_algorithmSeed->setValue(75);
    ui->spinBox_executionTimes->setRange(1, 100);
    ui->spinBox_executionTimes->setValue(1);
}

void knapsackWindow::on_pushButton_problemFile_clicked()
{
    try {
        QString problemFile = QFileDialog::getOpenFileName(
            this,
            tr("Open File"),
            "./../input/",
            tr("Text Files (*.txt);;Knapsack Files (*.knapsack)")
        );

        if (!problemFile.isEmpty()) {
            m_problemInstance = FILE_PROCESSOR::loadProblem(problemFile.toStdString());
            QString problemPrint = QString::fromStdString(m_problemInstance.toString());
            ui->plainTextEdit_problem->setPlainText(problemPrint);
            ui->pushButton_problemFile->setText(problemFile);
        }
    } catch (const std::exception &e) {
        ui->pushButton_problemFile->setText("Select an instance");
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
    ui->pushButton_problemFile->setEnabled(false);
    ui->pushButton_stop->setEnabled(true);
    m_stopRequested = false;

    // --- Set display formats once ---
    ui->timeEdit_estimatedTotalTime->setDisplayFormat("hh:mm:ss.zzz");
    ui->timeEdit_executionTimeOverall->setDisplayFormat("hh:mm:ss.zzz");

    // --- Parameters from UI ---
    QTime time = ui->timeEdit_maxExecutionTime->time();
    double maxExecutionTime = time.minute() * 60 + time.second();
    int seed = ui->spinBox_algorithmSeed->value();
    int maxExecutions = ui->spinBox_executionTimes->value();
    std::string timestamp = QDateTime::currentDateTime()
                                .toString("yyyy-MM-dd_HH-mm-ss")
                                .toStdString();

    QFileInfo fileInfo(ui->pushButton_problemFile->text());
    QString folderPath = fileInfo.absolutePath();
    QString fileName = fileInfo.fileName();

    ProblemInstance problemCopy = m_problemInstance;
    auto start_time = std::chrono::steady_clock::now();

    // --- Background run (non-blocking) ---
    m_future = QtConcurrent::run([=, this]() mutable {

        auto resetUI = [this]() {
            QMetaObject::invokeMethod(this, [=]() {
                ui->pushButton_stop->setText("Stop");
                ui->pushButton_stop->setEnabled(false);
                ui->pushButton_findBag->setEnabled(true);
                ui->pushButton_problemFile->setEnabled(true);
                ui->progressBar->setValue(0);
            }, Qt::QueuedConnection);
        };

        Algorithm algorithm(maxExecutionTime - 1, seed);
        std::unique_ptr<Bag> bestBagOverall = nullptr;
        int bestBenefitOverall = std::numeric_limits<int>::min();

        for (int execution = 0; execution < maxExecutions; ++execution) {
            if (m_stopRequested) break;

            auto exec_start = std::chrono::steady_clock::now();

            auto resultBags = algorithm.run(problemCopy, timestamp + 
                " | execution: " + std::to_string(execution + 1));

            auto exec_end = std::chrono::steady_clock::now();
            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                exec_end - exec_start
            );

            // Estimate total time based on first execution
            if (execution == 0) {
                auto estimatedTotalMs = elapsedMs.count() * maxExecutions;
                QMetaObject::invokeMethod(this, [=]() {
                    ui->timeEdit_estimatedTotalTime->setTime(
                        QTime::fromMSecsSinceStartOfDay(static_cast<int>(estimatedTotalMs % 86400000))
                    );
                }, Qt::QueuedConnection);
            }

            // Save results of this execution
            FILE_PROCESSOR::saveData(resultBags, folderPath.toStdString(), fileName.toStdString(), timestamp);

            // --- Check all bags in this execution ---
            for (const auto& bag : resultBags) {
                int currentBenefit = bag->getBenefit();
                if (currentBenefit > bestBenefitOverall) {
                    bestBenefitOverall = currentBenefit;
                    bestBagOverall = std::make_unique<Bag>(*bag);
                }
            }

            // Update progress
            int progressValue = static_cast<int>((100.0 * (execution + 1)) / maxExecutions);
            QMetaObject::invokeMethod(ui->progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, progressValue));

            // Update overall elapsed time
            auto now = std::chrono::steady_clock::now();
            auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            QMetaObject::invokeMethod(this, [=]() {
                ui->timeEdit_executionTimeOverall->setTime(
                    QTime::fromMSecsSinceStartOfDay(static_cast<int>(durationMs % 86400000))
                );
            }, Qt::QueuedConnection);
        }

        std::string reportFile = "";
        std::string validation = "";
        // --- Save the best bag overall ---
        if (bestBagOverall) {
            reportFile = FILE_PROCESSOR::saveReport(bestBagOverall, m_problemInstance.getPackages(),
            m_problemInstance.getDependencies(), seed, timestamp, folderPath.toStdString(), fileName.toStdString());
        }
        resetUI();;
    });
}

void knapsackWindow::on_pushButton_stop_clicked()
{
    m_stopRequested = true;
    ui->pushButton_stop->setEnabled(false);
    ui->pushButton_stop->setText("Stopping...");
}


void knapsackWindow::on_pushButton_reportFile_clicked()
{
    try {
        QString m_reportFile = QFileDialog::getOpenFileName(
            this,
            tr("Open File"),
            "./../input/",
            tr("Text Files (*.txt);;Knapsack Files (*.knapsack)")
        );

        if (!m_reportFile.isEmpty()) {
            ui->pushButton_reportFile->setText(m_reportFile);
            SolutionReport solutionReport= FILE_PROCESSOR::loadReport(m_reportFile.toStdString());
            ui->plainTextEdit_report->setPlainText(QString::fromStdString(solutionReport.toString()));
        }
    } catch (const std::exception &e) {
        ui->pushButton_reportFile->setText("Select a report file");
        QMessageBox::critical(this, "Error", QString("Failed to load report file:\n%1").arg(e.what()));
    } catch (...) {
        QMessageBox::critical(this, "Error", "An unknown error occurred.");
    }
}

void knapsackWindow::on_pushButton_validateReport_clicked()
{
    std::string problemFile = ui->pushButton_problemFile->text().toStdString();
    std::string reportFile = ui->pushButton_reportFile->text().toStdString();
    std::string validation = FILE_PROCESSOR::validateSolution(problemFile, reportFile);
    ui->plainTextEdit_reportValidation->setPlainText(QString::fromStdString(validation));
}

