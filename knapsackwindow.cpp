#include "knapsackwindow.h"
#include "ui_knapsackwindow.h"

#include <memory>
#include <exception>

#include <QString>
#include <QFileDialog>
#include <QMessageBox>


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

void knapsackWindow::initializeUi(){
    ui->pushButton_problem->setText("select a instance");
    ui->timeEdit_maxExecutionTime->setDisplayFormat("mm:ss");
    ui->timeEdit_maxExecutionTime->setTime(QTime(0, 1, 30));
    ui->timeEdit_maxExecutionTime->setTimeRange(QTime(0,0,0,0), QTime(0,59,59));
    ui->spinBox_algorithmSeed->setValue(75);
    ui->spinBox_algorithmSeed->setMinimum(0);
    ui->spinBox_algorithmSeed->setMaximum(100);
    ui->spinBox_executionTimes->setValue(5);
    ui->spinBox_executionTimes->setMinimum(1);
    ui->spinBox_executionTimes->setMaximum(100);
}

void knapsackWindow::on_pushButton_problem_clicked()
{
    try{
        QString problemFile = QFileDialog::getOpenFileName(
            this,
            tr("Open File"), 
            "C:/Users/alang/OneDrive/Documentsqcode/Knapsack-problem/input/",
            tr("Text Files (*.txt);;Knapsack Files (*.knapsack)")
        );

        if (!problemFile.isEmpty()) {
            m_problemInstance = FileProcessor::loadProblem(problemFile.toStdString());
            QString problemPrint = QString::fromStdString(m_problemInstance.toString());
            ui->plainTextEdit_problem->setPlainText(problemPrint);
            ui->pushButton_problem->setText(problemFile);
        }
    } catch(const std::exception& e) {
        // 1. Reset the UI
        ui->pushButton_problem->setText("select a instance");
        ui->plainTextEdit_problem->setPlainText("Error loading problem.");

        // 2. Show the user WHAT went wrong
        QMessageBox::critical(this, "Error", 
            QString("Failed to load problem file:\n%1").arg(e.what()));

    } catch(...) { // A "catch-all" for any other unknown exceptions
        QMessageBox::critical(this, "Error", "An unknown error occurred.");
    }
}


void knapsackWindow::on_pushButton_findBag_clicked()
{
    QTime time = ui->timeEdit_maxExecutionTime->time();
    int maxExecutionTime = time.second() + time.minute() * 60;
    int seed = ui->spinBox_algorithmSeed->text().toInt();
    Algorithm algorithm(maxExecutionTime, seed);

    int maxExecutions = ui->spinBox_executionTimes->text().toInt();
    int execution = 0;
    std::string timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    
    QFileInfo fileInfo(ui->pushButton_problem->text());
    QString folderPath = fileInfo.absolutePath();

    while(execution != maxExecutions){
        std::vector<std::unique_ptr<Bag>> bagsExecution = algorithm.run(m_problemInstance, timestamp);
        
        FileProcessor::saveData(bagsExecution, folderPath.toStdString(), fileInfo.fileName().toStdString());
        ++execution;
    }
}

