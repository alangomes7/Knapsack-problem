#include "knapsackwindow.h"
#include "./ui_knapsackwindow.h"
#include "fileprocessor.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>

#include <chrono>
#include <string>

#include "algorithm.h"

KnapsackWindow::KnapsackWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::KnapsackWindow),
      m_bagSize(0) {
    ui->setupUi(this);
    initializeUiElements();
    setupConnections();
}

KnapsackWindow::~KnapsackWindow() {
    // Clean up all dynamically allocated objects to prevent memory leaks.
    for (Bag* bag : m_bags) {
        delete bag;
    }
    for (auto const& [key, val] : m_availablePackages) {
        delete val;
    }
    for (auto const& [key, val] : m_availableDependencies) {
        delete val;
    }
    delete ui;
}

void KnapsackWindow::initializeUiElements()
{
    m_bagSize = 0;
    ui->plainTextEdit_logs->setReadOnly(true);
    ui->comboBox_algorithm->addItem("Select Algorithm");
    ui->label_bagCapacityNumber->setText(QString::number(m_bagSize) + " MB");
    int executionTime = 5;
    for(int i = 1; i <= 3; i++) {
        ui->comboBox_executionTime->addItem(QString::number(executionTime * i) + " s");
    }
}

void KnapsackWindow::setupConnections() {
    connect(ui->pushButton_filePath, &QPushButton::clicked, this, &KnapsackWindow::onFilePathButtonClicked);
    connect(ui->pushButton_run, &QPushButton::clicked, this, &KnapsackWindow::onRunButtonClicked);
    connect(ui->comboBox_algorithm, &QComboBox::currentTextChanged, this, &KnapsackWindow::onAlgorithmChanged);
}

void KnapsackWindow::onFilePathButtonClicked() {
    QString filter = "Knapsack Files (*.knapsack.txt)";
    m_filePath = QFileDialog::getOpenFileName(this, "Open Knapsack File", "./../../input", filter);

    if (m_filePath.isEmpty()) {
        return;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "Could not open file. Please check permissions.");
        m_filePath.clear();
        return;
    }
    file.close();

    ui->pushButton_filePath->setText(m_filePath);
    loadFile();
}

void KnapsackWindow::onRunButtonClicked() {
    for (Bag* bag : m_bags) {
        delete bag;
    }
    m_bags.clear();

    std::vector<Package*> packagesVec;
    packagesVec.reserve(m_availablePackages.size());
    for(auto const& [key, val] : m_availablePackages) {
        packagesVec.push_back(val);
    }

    std::vector<Dependency*> dependenciesVec;
    dependenciesVec.reserve(m_availableDependencies.size());
    for(auto const& [key, val] : m_availableDependencies) {
        dependenciesVec.push_back(val);
    }

    std::string timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    int executionTime = (ui->comboBox_executionTime->currentText().remove("s")).toInt();

    m_algorithm = new Algorithm(executionTime);
    m_bags = m_algorithm->run(Algorithm::ALGORITHM_TYPE::RANDOM, m_bagSize, packagesVec, dependenciesVec, timeStamp);

    // Clear and populate ComboBox with normalized labels
    ui->comboBox_algorithm->clear();
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GREEDY_Package_Size)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::VND)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::VNS, Algorithm::LOCAL_SEARCH::FIRST_IMPROVEMENT)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::VNS, Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::VNS, Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT)));

    saveData();
}

void KnapsackWindow::onAlgorithmChanged() {
    printBag(ui->comboBox_algorithm->currentText().toStdString());
}

std::string KnapsackWindow::getAlgorithmLabel(Algorithm::ALGORITHM_TYPE algo, Algorithm::LOCAL_SEARCH ls) {
    std::string label = m_algorithm->toString(algo);
    if (ls != Algorithm::LOCAL_SEARCH::NONE) {
        label += " | " + m_algorithm->toString(ls);
    }
    return label;
}

void KnapsackWindow::saveData()
{
    if (m_bags.empty()) return;

    QFile inputFile(m_filePath);
    QFileInfo inputInfo(inputFile);

    const QString csvFile = inputInfo.absolutePath() + "/results_" + "knapsackResults.csv";

    QFile file(csvFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Error: Could not create or open" << csvFile;
        return;
    }

    QTextStream out(&file);
    if (file.size() == 0) {
        out << "Algoritmo,File name,Timestamp,Tempo de Processamento (s),Pacotes,Depenências,Peso da mochila,Benefício mochila\n";
    }
    int bagNumber = static_cast<int>(m_bags.size());
    int i = 0;
    for (const Bag *bagData : m_bags) {
        if (!bagData) continue;

        Algorithm algorithmHelper(0);
        QString algoritmo = QString::fromStdString(
            getAlgorithmLabel(bagData->getBagAlgorithm(), bagData->getBagLocalSearch())
        );
        QString fileName      = QFileInfo(m_filePath).fileName();
        QString timestamp     = QString::fromStdString(bagData->getTimestamp());
        double processingTime = bagData->getAlgorithmTime();
        int packages          = bagData->getPackages().size();
        int dependencies      = bagData->getDependencies().size();
        int bagWeight         = bagData->getSize();
        int bagBenefit        = bagData->getBenefit();

        out << algoritmo << ","
            << fileName << ","
            << timestamp << ","
            << QString::number(processingTime, 'f', 5) << ","
            << packages << ","
            << dependencies << ","
            << bagWeight << ","
            << bagBenefit;
        if (i < bagNumber){
            out << "\n";
        }
        i++;
    }

    file.close();
    qDebug() << "CSV written to" << csvFile;
}

void KnapsackWindow::loadFile() {
    FileProcessor fileProcessor(m_filePath, this);
    const auto& processedData = fileProcessor.getProcessedData();

    if (processedData.size() < 3) {
        QMessageBox::warning(this, "Error", "Invalid file format.");
        return;
    }

    for (auto const& [key, val] : m_availablePackages) { delete val; }
    m_availablePackages.clear();
    for (auto const& [key, val] : m_availableDependencies) { delete val; }
    m_availableDependencies.clear();

    m_bagSize = processedData.at(0).at(3).toInt();
    const auto& packagesData = processedData.at(1);
    const auto& dependenciesData = processedData.at(2);

    for (int i = 0; i < dependenciesData.size(); ++i) {
        std::string depName = std::to_string(i);
        int size = dependenciesData.at(i).toInt();
        m_availableDependencies[depName] = new Dependency(depName, size);
    }

    for (int i = 0; i < packagesData.size(); ++i) {
        std::string pkgName = std::to_string(i);
        int benefit = packagesData.at(i).toInt();
        m_availablePackages[pkgName] = new Package(pkgName, benefit);
    }

    for (size_t i = 3; i < processedData.size(); ++i) {
        const auto& line = processedData.at(i);
        if (line.size() != 2) continue;

        std::string pkgName = line.at(0).toStdString();
        std::string depName = line.at(1).toStdString();

        auto pkgIt = m_availablePackages.find(pkgName);
        auto depIt = m_availableDependencies.find(depName);

        if (pkgIt != m_availablePackages.end() && depIt != m_availableDependencies.end()) {
            Package* package = pkgIt->second;
            Dependency* dependency = depIt->second;
            package->addDependency(*dependency);
            dependency->addAssociatedPackage(package);
        }
    }

    ui->plainTextEdit_logs->clear();
    ui->label_SoftwarePackagesFileNumber->setText(QString::number(m_availablePackages.size()));
    ui->label_SoftwareDepenciesFileNumber->setText(QString::number(m_availableDependencies.size()));
    ui->label_maxBagCapacityNumber->setText(QString::number(m_bagSize) + " MB");
    printPackages();
    printDependencies();
}

void KnapsackWindow::printPackages() {
    QString log_text = "List of all available packages:\n- - -\n";
    for (const auto& pair : m_availablePackages) {
        const Package* package = pair.second;
        log_text.append(QString::fromStdString(package->toString()) + "\n- - -\n");
    }
    ui->plainTextEdit_logs->setPlainText(log_text);
}

void KnapsackWindow::printDependencies() {
    QString log_text = "List of all available dependencies:\n- - -\n";
    for (const auto& pair : m_availableDependencies) {
        const Dependency* dependency = pair.second;
        log_text.append("Dependency: " + QString::fromStdString(dependency->getName()) + "\n");
        log_text.append("Size: " + QString::number(dependency->getSize()) + " MB\n");

        const auto& associatedPackages = dependency->getAssociatedPackages();
        if (!associatedPackages.empty()) {
            log_text.append("Associated Packages:\n");
            for (const auto& pkg_pair : associatedPackages) {
                const Package* associatedPackage = pkg_pair.second;
                log_text.append("  - " + QString::fromStdString(associatedPackage->getName()) + "\n");
            }
        }
        log_text.append("\n- - -\n");
    }
    ui->plainTextEdit_logs->setPlainText(log_text);
}

void KnapsackWindow::printBag(const std::string& algorithmName) {
    if (m_bags.empty()) return;

    const Bag* bagToPrint = nullptr;

    for (const Bag* selectedBag : m_bags) {
        std::string currentBagAlgorithmName = getAlgorithmLabel(
            selectedBag->getBagAlgorithm(),
            selectedBag->getBagLocalSearch()
        );
        if (currentBagAlgorithmName == algorithmName) {
            bagToPrint = selectedBag;
            break;
        }
    }

    if (!bagToPrint) return;

    ui->plainTextEdit_logs->clear();
    QString log_text = QString::fromStdString(bagToPrint->toString());
    ui->plainTextEdit_logs->setPlainText(log_text);

    ui->label_PackedSoftwarePackagesNumber->setText(QString::number(bagToPrint->getPackages().size()));
    ui->label_PackedSoftwareDependenciesNumber->setText(QString::number(bagToPrint->getDependencies().size()));
    ui->label_bagCapacityNumber->setText(QString::number(bagToPrint->getSize()) + " MB");
    ui->label_bagBenefitNumber->setText(QString::number(bagToPrint->getBenefit()) + " MB");
    ui->label_processingTimeNumber->setText(QString::number(bagToPrint->getAlgorithmTime()) + " s");
}
