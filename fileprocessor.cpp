#include "FileProcessor.h"
#include "algorithm.h"
#include "bag.h"
#include "dependency.h"
#include "package.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <unordered_set>

FileProcessor::FileProcessor(const std::string& filePath)
    : fileinputPath(filePath)
{
    m_output_dir = fileinputPath + "/output";
    m_output_dir = backslashesPathToSlashesPath(m_output_dir);
    try {
        // Check if the directory exists
        if (std::filesystem::exists(m_output_dir)) {
            std::cout << "Directory '" << m_output_dir << "' already exists." << std::endl;
        } else {
            // Create the directory
            if (std::filesystem::create_directory(m_output_dir)) {
                std::cout << "Directory '" << m_output_dir << "' created successfully." << std::endl;
            } else {
                std::cerr << "Error: Failed to create directory '" << m_output_dir << "'." << std::endl;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cout << "Filesystem error: " << e.what() << std::endl;
    }
    readFile();
}

const std::string FileProcessor::getPath() const
{
    return fileinputPath;
}

const std::vector<std::vector<std::string>>& FileProcessor::getProcessedData() const
{
    return m_processedData;
}

void FileProcessor::readFile()
{
    std::ifstream fileinput(fileinputPath);

    // Check if the file was opened successfully.
    if (!fileinput.is_open()) {
        std::cerr << "Error: Could not open file " << fileinputPath << std::endl;
        return;
    }

    std::string line;
    // Read the file line by line.
    while (std::getline(fileinput, line)) {
        std::stringstream ss(line);
        std::string word;
        std::vector<std::string> words;

        // Split the line into words based on whitespace.
        while (ss >> word) {
            words.push_back(word);
        }

        // Add the list of words to our data structure if the line was not empty.
        if (!words.empty()) {
            m_processedData.push_back(words);
        }
    }
}

void FileProcessor::saveData(const std::vector<Bag*>& bags)
{
    if (bags.empty()) return;

    // Construct the output CSV file path using std::filesystem.
    std::filesystem::path inputPath(fileinputPath);
    const std::string csvFile = m_output_dir + "/results_knapsackResults.csv";

    // Open the file in append mode.
    std::ofstream outFile(csvFile, std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not create or open " << csvFile << std::endl;
        return;
    }

    // Write header only if the file is new or empty.
    outFile.seekp(0, std::ios::end);
    if (outFile.tellp() == 0) {
        outFile << "Algoritmo,Movement,File name,Timestamp,Tempo de Processamento (s),Pacotes,Depenências,Peso da mochila,Benefício mochila\n";
    }

    // Write data for each bag (solution).
    for (const Bag *bagData : bags) {
        if (!bagData) continue;

        Algorithm algorithmHelper(0);
        std::string algoritmo = algorithmHelper.toString(bagData->getBagAlgorithm());
        std::string localSearch = algorithmHelper.toString(bagData->getBagLocalSearch());
        if(localSearch != "None"){
            algoritmo = algoritmo + " | " + localSearch;
        }
        std::string movement = bagData->toString(bagData->getMovementType());
        std::string fileName = inputPath.filename().string();
        std::string timestamp = bagData->getTimestamp();
        double processingTime = bagData->getAlgorithmTime();

        outFile << algoritmo << ","
                << movement << ","
                << fileName << ","
                << timestamp << ","
                << std::fixed << std::setprecision(5) << processingTime << ","
                << bagData->getPackages().size() << ","
                << bagData->getDependencies().size() << ","
                << bagData->getSize() << ","
                << bagData->getBenefit() << "\n";
    }

    outFile.close();
    std::cout << "CSV written to " << csvFile << std::endl;
}

void FileProcessor::saveReport(const std::vector<Bag*>& bags,
                                const std::unordered_map<std::string, Package*>& allPackages,
                                const std::unordered_map<std::string, Dependency*>& allDependencies,
                                unsigned int seed, const std::string& timestamp) {
    if (bags.empty()) {
        std::cerr << "Error: Cannot generate report from an empty set of solutions." << std::endl;
        return;
    }

    // Find the best bag (solution with the highest benefit).
    auto bestBagIt = std::max_element(bags.begin(), bags.end(), [](const Bag* a, const Bag* b) {
        return a->getBenefit() < b->getBenefit();
    });
    const Bag* bestBag = *bestBagIt;

    // Create a unique report filename.
    std::filesystem::path inputPath(fileinputPath);
    std::string directory = inputPath.parent_path().string();
    std::string stem = inputPath.stem().string();
    std::string reportFileName = directory + "/report_" + stem + "_" + formatTimestampForFilename(timestamp) + ".txt";

    // Write the report file.
    std::ofstream outFile(reportFileName);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open " << reportFileName << " for writing." << std::endl;
        return;
    }

    outFile << "--- Experiment Reproduction Report for " << inputPath.filename().string() << " ---\n";

    Algorithm algorithmAux(0);
    outFile << "Methaeuristic: " << algorithmAux.toString(bestBag->getBagAlgorithm()) + " | " + algorithmAux.toString(bestBag->getBagLocalSearch()) << "\n";
    outFile << "Benefit Value: " << bestBag->getBenefit() << "\n";
    outFile << "Total Disk Space (Dependencies): " << bestBag->getSize() << " MB\n";
    outFile << "Solution (Binary Vectors):\n";

    // --- Packages Vector ---
    std::unordered_set<std::string> solutionPackages;
    for (const auto& pkg : bestBag->getPackages()) {
        solutionPackages.insert(pkg->getName());
    }

    std::vector<std::string> allPackageNames;
    allPackageNames.reserve(allPackages.size());
    for (const auto& pair : allPackages) {
        allPackageNames.push_back(pair.first);
    }
    std::sort(allPackageNames.begin(), allPackageNames.end());

    outFile << "[";
    for (size_t i = 0; i < allPackageNames.size(); ++i) {
        outFile << (solutionPackages.count(allPackageNames[i]) ? "1" : "0")
                << (i == allPackageNames.size() - 1 ? "" : ", ");
    }
    outFile << "]\n";

    // --- Dependencies Vector ---
    std::unordered_set<std::string> solutionDependencies;
    for (const auto& dep : bestBag->getDependencies()) {
        solutionDependencies.insert(dep->getName());
    }

    std::vector<std::string> allDependencyNames;
    allDependencyNames.reserve(allDependencies.size());
    for (const auto& pair : allDependencies) {
        allDependencyNames.push_back(pair.first);
    }
    std::sort(allDependencyNames.begin(), allDependencyNames.end());
    
    outFile << "[";
    for (size_t i = 0; i < allDependencyNames.size(); ++i) {
        outFile << (solutionDependencies.count(allDependencyNames[i]) ? "1" : "0")
                << (i == allDependencyNames.size() - 1 ? "" : ", ");
    }
    outFile << "]\n";

    // --- Report Footer ---
    outFile << "Metaheuristic Parameters: " << (bestBag->getMetaheuristicParameters().empty() ? "N/A" : bestBag->getMetaheuristicParameters()) << "\n";
    outFile << "Random Seed: " << seed << "\n";
    outFile << "Execution Time: " << std::fixed << std::setprecision(5) << bestBag->getAlgorithmTime() << " seconds\n";
    outFile << "Timestamp: " << timestamp << "\n";
    
    outFile.close();
    std::cout << "\nDetailed report saved to " << reportFileName << std::endl;
}

std::string FileProcessor::backslashesPathToSlashesPath(std::string backslashesPath){
    std::string slashesPath = backslashesPath;
    std::replace(slashesPath.begin(), slashesPath.end(), '\\', '/');
    return slashesPath;
}

std::string FileProcessor::formatTimestampForFilename(std::string timestamp) {
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');
    std::replace(timestamp.begin(), timestamp.end(), ':', '_');
    return timestamp;
}