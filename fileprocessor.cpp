#include "FileProcessor.h"
#include "algorithm.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <unordered_set>

#include "bag.h"
#include "package.h"
#include "dependency.h"

FileProcessor::FileProcessor(const std::string& filePath)
    : fileinputPath(filePath)
{
    std::filesystem::path inputPath(fileinputPath);
    std::string directory = inputPath.parent_path().string();
    
    // Base output folder
    m_output_dir = backslashesPathToSlashesPath(directory + "/output");
    
    try {
        int counter = 1;
        std::string newOutputDir = m_output_dir;
        
        // Keep iterating until we successfully create a new folder
        while (std::filesystem::exists(newOutputDir)) {
            std::ostringstream oss;
            oss << m_output_dir << "-" << counter++;
            newOutputDir = oss.str();
        }
        
        if (std::filesystem::create_directory(newOutputDir)) {
            m_output_dir = newOutputDir;
            std::cout << "Directory '" << m_output_dir << "' created successfully." << std::endl;
        } else {
            std::cerr << "Error: Failed to create directory '" << newOutputDir << "'." << std::endl;
        }
        
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
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

    std::filesystem::path inputPath(fileinputPath);
    std::string fileName = inputPath.filename().string();

    // Write data for each bag (solution).
    for (const Bag *bagData : bags) {
        if (!bagData) continue;

        Algorithm algorithmHelper(0);
        std::string algoritmo = algorithmHelper.toString(bagData->getBagAlgorithm());
        std::string localSearch = algorithmHelper.toString(bagData->getBagLocalSearch());
        if(localSearch != "None"){
            algoritmo = algoritmo + " | " + localSearch;
        }
        std::string movement =  bagData->toString(bagData->getMovementType());
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

    /**
     * @brief Saves a detailed report of the best solution found.
     * @param bags A vector of Bag pointers, representing all the solutions found.
     * @param allPackages A vector containing pointers to all possible packages.
     * @param allDependencies A vector containing pointers to all possible dependencies.
     * @param seed The random seed used for the experiment.
     * @param timestamp The timestamp of when the experiment was run.
     */
    void FileProcessor::saveReport(const std::vector<Bag*>& bags,
                                const std::vector<Package*>& allPackages,
                                const std::vector<Dependency*>& allDependencies,
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
    const std::string csvFile = m_output_dir + "/results_knapsackResults.csv";

    std::filesystem::path inputPath(fileinputPath);
    std::string output_dir = m_output_dir;
    if (output_dir.empty()) {
        output_dir = ".";
    }

    std::string stem = inputPath.stem().string();
    std::string reportFileName = output_dir + "/report_" + stem + "_" + formatTimestampForFilename(timestamp) + ".txt";

    // Write the report file.
    std::ofstream outFile(reportFileName);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open " << reportFileName << " for writing." << std::endl;
        return;
    }

    outFile << "--- Experiment Reproduction Report for " << inputPath.filename().string() << " ---\n";

    Algorithm algorithmAux(0);
    std::string algoritmo = algorithmAux.toString(bestBag->getBagAlgorithm());
    std::string localSearch = algorithmAux.toString(bestBag->getBagLocalSearch());
    if(localSearch != "None"){
        algoritmo = algoritmo + " | " + localSearch;
    }
    outFile << "Methaeuristic: " << algoritmo << "\n";
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
    // *** MODIFIED HERE: Iterate over vector of pointers ***
    for (const auto& pkg : allPackages) {
        allPackageNames.push_back(pkg->getName());
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
    // *** MODIFIED HERE: Iterate over vector of pointers ***
    for (const auto& dep : allDependencies) {
        allDependencyNames.push_back(dep->getName());
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