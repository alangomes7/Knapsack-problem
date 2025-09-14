#include "knapsackwindow.h"
#include "./ui_knapsackwindow.h"
#include "fileprocessor.h"

#include <QFileDialog>
#include <QMessageBox>

KnapsackWindow::KnapsackWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::KnapsackWindow)
{
    ui->setupUi(this);
    connections();
    initializeUiElements();
}

KnapsackWindow::~KnapsackWindow()
{
    qDeleteAll(m_available_depencies);
    delete ui;
}

void KnapsackWindow::initializeUiElements()
{
    bag_size = 0;
    ui->plainTextEdit_logs->setReadOnly(true);
    ui->comboBox_algorithm->addItem("Algorithm - aleatório");
    ui->comboBox_algorithm->addItem("Algorithm - guloso");
    ui->comboBox_algorithm->addItem("Algorithm - guloso randomizado");
    ui->label_bagCapacityNumber->setText(QString::number(bag_size) + " MB");
}

void KnapsackWindow::connections()
{
    connect(ui->pushButton_filePath, &QPushButton::clicked, this, &KnapsackWindow::pushButton_filePath_clicked);
    connect(ui->pushButton_run, &QPushButton::clicked, this, &KnapsackWindow::pushButton_run_clicked);
}

void KnapsackWindow::pushButton_filePath_clicked()
{
    QString filter = "txt Files (*.knapsack.txt)";
    m_filePath = QFileDialog::getOpenFileName(this, "Open knapsack files", "", filter);

    if (m_filePath.isEmpty()) {
        return;
    }

    QFile m_fileInput(m_filePath);
    if (!m_fileInput.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not open file. Please check if it exists and you have read permissions."));
        m_filePath.clear();
        return;
    }
    m_fileInput.close();
    ui->pushButton_filePath->setText(m_filePath);
    loadFile();
}

void KnapsackWindow::pushButton_run_clicked()
{
}

void KnapsackWindow::loadFile()
{
    FileProcessor fileProcessor(m_filePath, this);
    const QList<QStringList> processedData = fileProcessor.getProcessedData();

    if (processedData.size() < 3) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid file format."));
        return;
    }

    bag_size = processedData.at(0).at(3).toInt();
    QStringList packages = processedData.at(1);
    QStringList dependencies = processedData.at(2);

    // Load dependencies
    qDeleteAll(m_available_depencies);
    m_available_depencies.clear();
    for (int i = 0; i < dependencies.size(); ++i) {
        int size = dependencies.at(i).toInt();
        Dependency *dep = new Dependency(QString::number(i), size, this);
        m_available_depencies.append(dep);
    }

    // Load packages
    qDeleteAll(m_unsacked_packages);
    m_unsacked_packages.clear();
    for (int i = 0; i < packages.size(); ++i) {
        int benefit = packages.at(i).toInt();
        Package *pkg = new Package(QString::number(i), benefit, this);
        m_unsacked_packages.append(pkg);
    }

    // Load relations (package → dependency)
    for (int i = 3; i < processedData.size(); ++i) {
        const QStringList &line = processedData.at(i);

        if (line.size() != 2)
            continue;

        int packageIndex = line.at(0).toInt();
        int dependencyIndex = line.at(1).toInt();

        if (packageIndex >= 0 && packageIndex < m_unsacked_packages.size() &&
            dependencyIndex >= 0 && dependencyIndex < m_available_depencies.size())
        {
            m_unsacked_packages.at(packageIndex)->addDependency(m_available_depencies.at(dependencyIndex));
            m_available_depencies.at(dependencyIndex)->addAssociatedPackage(m_unsacked_packages.at(packageIndex));
        }
    }
    ui->plainTextEdit_logs->clear();
    ui->label_SoftwarePackagesFileNumber->setText(QString::number(m_unsacked_packages.size()));
    ui->label_SoftwareDepenciesFileNumber->setText(QString::number(m_available_depencies.size()));
    ui->label_bagCapacityNumber->setText(QString::number(bag_size) + " MB");
    printPackages();
    printDependecies();
}

void KnapsackWindow::printPackages()
{
    QString log_text = "Lista de pacotes lidos: \n - - - \n";
     for (int i = 0; i < m_unsacked_packages.size(); ++i) {
        Package *package = m_unsacked_packages.at(i);
        log_text.append(package->toQString() + QString::fromStdString("\n - - - \n"));
     }
    ui->plainTextEdit_logs->appendPlainText(log_text);
}

void KnapsackWindow::printDependecies()
{
    QString log_text = "Lista de dependencias: \n - - - \n";
    for (int i = 0; i < m_available_depencies.size(); ++i) {
        Dependency* dependency = m_available_depencies.at(i);
        log_text.append("Dependency: " + dependency->getName() + "\n");
        log_text.append("Single size: " + QString::number(dependency->getSize()) + " MB\n");

        QString associatedPackages;
        for(int j = 0; j < dependency->getAssociatedPackages().size(); j++){
            Package* associatedPackage = dependency->getAssociatedPackages().at(j);
            associatedPackages += "Package: " + associatedPackage->getName() + " | Benefit: " + QString::number(associatedPackage->getBenefit()) + "MB\n";
        }
        log_text.append(associatedPackages);
        log_text.append("Max size: "+ QString::number(dependency->getMaxSize()) + " MB");
        log_text.append("\n - - - \n");
     }
    ui->plainTextEdit_logs->appendPlainText(log_text);
}
