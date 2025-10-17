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
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>

KnapsackWindow::KnapsackWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::KnapsackWindow),
      m_bagSize(0) {
    ui->setupUi(this);
    initializeUiElements();
    setupConnections();
}

KnapsackWindow::~KnapsackWindow() {
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
    QIntValidator* validator = new QIntValidator(0, 100, this);
    ui->lineEdit_seed->setValidator(validator);

    ui->lineEdit_seed->setText(QString::number(50));

    ui->plainTextEdit_logs->setReadOnly(true);
    ui->comboBox_algorithm->addItem("Select Algorithm");
    ui->label_bagCapacityNumber->setText(QString::number(m_bagSize) + " MB");
    int executionTime = 30;
    for(int i = 2; i <= 4; i++){
        ui->comboBox_executionTime->addItem(QString::number(executionTime) + " s");
        executionTime = executionTime * i;
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

    std::string timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss").toStdString();
    int executionTime = (ui->comboBox_executionTime->currentText().remove("s")).toInt();

    m_algorithm = new Algorithm(executionTime);
    m_bags = m_algorithm->run(Algorithm::ALGORITHM_TYPE::RANDOM, m_bagSize, packagesVec, dependenciesVec, timestamp);

    // Clear and populate ComboBox with normalized labels
    ui->comboBox_algorithm->clear();
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GREEDY_Package_Size)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::VND, Algorithm::LOCAL_SEARCH::NONE)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::VNS, Algorithm::LOCAL_SEARCH::NONE)));
    ui->comboBox_algorithm->addItem(QString::fromStdString(getAlgorithmLabel(Algorithm::ALGORITHM_TYPE::GRASP, Algorithm::LOCAL_SEARCH::NONE)));

    FileProcessor fileProcessor(m_filePath.toStdString());
    fileProcessor.saveData(m_bags);
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

void KnapsackWindow::loadFile() {
    FileProcessor fileProcessor(m_filePath.toStdString());
    const auto& processedData = fileProcessor.getProcessedData();

    if (processedData.size() < 3) {
        QMessageBox::warning(this, "Error", "Invalid file format.");
        return;
    }

    for (auto const& [key, val] : m_availablePackages) { delete val; }
    m_availablePackages.clear();
    m_availablePackages.reserve(std::stoi(processedData.at(0).at(0)));
    for (auto const& [key, val] : m_availableDependencies) { delete val; }
    m_availableDependencies.clear();
    m_availableDependencies.reserve(std::stoi(processedData.at(0).at(1)));

    m_bagSize = std::stoi(processedData.at(0).at(3));
    const auto& packagesData = processedData.at(1);
    const auto& dependenciesData = processedData.at(2);

    for (int i = 0; i < dependenciesData.size(); ++i) {
        std::string depName = std::to_string(i);
        int size = std::stoi(dependenciesData.at(i));
        m_availableDependencies[depName] = new Dependency(depName, size);
    }

    for (int i = 0; i < packagesData.size(); ++i) {
        std::string pkgName = std::to_string(i);
        int benefit = std::stoi(packagesData.at(i));
        m_availablePackages[pkgName] = new Package(pkgName, benefit);
    }

    for (size_t i = 3; i < processedData.size(); ++i) {
        const auto& line = processedData.at(i);
        if (line.size() != 2) continue;

        std::string pkgName = line.at(0);
        std::string depName = line.at(1);

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

    if (!bagToPrint){
        bagToPrint = new Bag(Algorithm::ALGORITHM_TYPE::NONE, "0");
    }

    ui->plainTextEdit_logs->clear();
    QString log_text = QString::fromStdString(bagToPrint->toString());
    ui->plainTextEdit_logs->setPlainText(log_text);

    ui->label_PackedSoftwarePackagesNumber->setText(QString::number(bagToPrint->getPackages().size()));
    ui->label_PackedSoftwareDependenciesNumber->setText(QString::number(bagToPrint->getDependencies().size()));
    ui->label_bagCapacityNumber->setText(QString::number(bagToPrint->getSize()) + " MB");
    ui->label_bagBenefitNumber->setText(QString::number(bagToPrint->getBenefit()) + " MB");
    ui->label_processingTimeNumber->setText(QString::number(bagToPrint->getAlgorithmTime()) + " s");
}