#include "main.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <unordered_set>

#include "algorithm.h"
#include "package.h"
#include "dependency.h"
#include "bag.h"

int main(int argc, char* argv[]) {
    std::string filePath;
    double maxTime;
    unsigned int seed;

    // --- User Input Section ---
    // std::cout << "Enter the path to the input file: ";
    // std::getline(std::cin, filePath);

    filePath = "C:/Users/alang/OneDrive/Documents/code/Knapsack-problem/input/sukp29_500_485_0.15_0.85.txt";
    std::cout << "Using file: " << filePath << std::endl;

    // std::cout << "Enter the maximum execution time in seconds: ";
    // std::cin >> maxTime;
    maxTime = 10;

    // std::cout << "Enter a random seed (an integer) for reproducibility: ";
    // std::cin >> seed;
    seed = 5;
    // --- End of User Input Section ---


    int bagSize = 0;
    std::unordered_map<std::string, Package*> availablePackages;
    std::unordered_map<std::string, Dependency*> availableDependencies;

    try {
        loadFile(filePath, bagSize, availablePackages, availableDependencies);
    } catch (const std::exception& e) {
        std::cerr << "Error loading file: " << e.what() << std::endl;
        return 1;
    }

    std::vector<Package*> packagesVec;
    for (auto const& [key, val] : availablePackages) {
        packagesVec.push_back(val);
    }

    std::vector<Dependency*> dependenciesVec;
    for (auto const& [key, val] : availableDependencies) {
        dependenciesVec.push_back(val);
    }

    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;

#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif

    std::stringstream ss;
    ss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    std::string timestamp = ss.str();

    Algorithm algorithm(maxTime, seed);
    std::vector<Bag*> bags = algorithm.run(Algorithm::ALGORITHM_TYPE::RANDOM, bagSize, packagesVec, dependenciesVec, timestamp);

    printResults(bags);
    saveData(bags, filePath);
    saveReport(bags, availablePackages, availableDependencies, seed, filePath, timestamp);

    // Cleanup
    for (Bag* bag : bags) {
        delete bag;
    }
    for (auto const& [key, val] : availablePackages) {
        delete val;
    }
    for (auto const& [key, val] : availableDependencies) {
        delete val;
    }

    return 0;
}

// ===================================================================
// == Function Implementations
// ===================================================================

void loadFile(const std::string& filePath, int& bagSize, std::unordered_map<std::string, Package*>& packages, std::unordered_map<std::string, Dependency*>& dependencies) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath);
    }

    std::string line;
    // Read header: m n ne b
    std::getline(file, line);
    std::stringstream headerStream(line);
    int m, n, ne;
    headerStream >> m >> n >> ne >> bagSize;

    // Read package benefits
    std::getline(file, line);
    std::stringstream packagesStream(line);
    for (int i = 0; i < m; ++i) {
        int benefit;
        packagesStream >> benefit;
        std::string name = std::to_string(i);
        packages[name] = new Package(name, benefit);
    }

    // Read dependency sizes
    std::getline(file, line);
    std::stringstream dependenciesStream(line);
    for (int i = 0; i < n; ++i) {
        int size;
        dependenciesStream >> size;
        std::string name = std::to_string(i);
        dependencies[name] = new Dependency(name, size);
    }

    // Read package-dependency relationships (edges)
    while (std::getline(file, line)) {
        if (line.empty() || line.find_first_not_of(" \t\n\v\f\r") == std::string::npos) {
            continue;
        }
        std::stringstream edgeStream(line);
        std::string pkgName, depName;
        edgeStream >> pkgName >> depName;

        if (packages.count(pkgName) && dependencies.count(depName)) {
            Package* package = packages.at(pkgName);
            Dependency* dependency = dependencies.at(depName);
            package->addDependency(*dependency);
            dependency->addAssociatedPackage(package);
        }
    }
}

void printResults(const std::vector<Bag*>& bags) {
    std::cout << "\n--- Algorithm Results ---" << std::endl;
    for (const auto& bag : bags) {
        if (bag) {
            std::cout << "----------------------------------------" << std::endl;
            std::cout << bag->toString() << std::endl;
        }
    }
    std::cout << "----------------------------------------" << std::endl;
}

void saveData(const std::vector<Bag*>& bags, const std::string& inputFileName) {
    if (bags.empty()) return;

    std::string outputFileName = "results_knapsackResults.csv";
    std::ofstream outFile(outputFileName, std::ios_base::app);

    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open " << outputFileName << " for writing." << std::endl;
        return;
    }
    
    // Check if the file is empty to write the header
    outFile.seekp(0, std::ios::end);
    if (outFile.tellp() == 0) {
        outFile << "Algoritmo,File name,Timestamp,Tempo de Processamento (s),Pacotes,Depenências,Peso da mochila,Benefício mochila\n";
    }

    Algorithm algorithmHelper(0);
    for (const Bag* bagData : bags) {
        if (!bagData) continue;

        std::string algoritmo = getAlgorithmLabel(algorithmHelper, bagData->getBagAlgorithm(), bagData->getBagLocalSearch());
        
        // Extract just the filename from the full path
        std::string base_filename = inputFileName.substr(inputFileName.find_last_of("/\\") + 1);
        
        outFile << algoritmo << ","
                << base_filename << ","
                << bagData->getTimestamp() << ","
                << std::fixed << std::setprecision(5) << bagData->getAlgorithmTime() << ","
                << bagData->getPackages().size() << ","
                << bagData->getDependencies().size() << ","
                << bagData->getSize() << ","
                << bagData->getBenefit() << "\n";
    }

    outFile.close();
    std::cout << "\nResults successfully appended to " << outputFileName << std::endl;
}

std::string getAlgorithmLabel(const Algorithm& algorithm, Algorithm::ALGORITHM_TYPE algo, Algorithm::LOCAL_SEARCH ls) {
    std::string label = algorithm.toString(algo);
    if (ls != Algorithm::LOCAL_SEARCH::NONE) {
        label += " | " + algorithm.toString(ls);
    }
    return label;
}

void saveReport(const std::vector<Bag*>& bags,
                                const std::unordered_map<std::string, Package*>& allPackages,
                                const std::unordered_map<std::string, Dependency*>& allDependencies,
                                unsigned int seed, const std::string& filePath,
                                const std::string& timestamp) {

    // 1. Find the best bag to generate the report.
    if (bags.empty()) {
        std::cerr << "Error: Cannot generate report from an empty set of solutions." << std::endl;
        return;
    }

    auto bestBagIt = std::max_element(bags.begin(), bags.end(), [](const Bag* a, const Bag* b) {
        return a->getBenefit() < b->getBenefit();
    });
    const Bag* bestBag = *bestBagIt;

    // 2. Create a unique and robust report filename.
    std::filesystem::path inputPath(filePath);
    std::string directory = inputPath.parent_path().string();
    std::string stem = inputPath.stem().string();
    std::string reportFileName = directory + "/report_" + stem + "_" + formatTimestampForFilename(timestamp) + ".txt";

    // 3. Write the report file.
    std::ofstream outFile(reportFileName);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open " << reportFileName << " for writing." << std::endl;
        return;
    }

    outFile << "--- Experiment Reproduction Report for " << inputPath.filename().string() << " ---" << std::endl;

    Algorithm algorithmAux(0);
    outFile << "Methaeuristic: " << getAlgorithmLabel(algorithmAux, bestBag->getBagAlgorithm(), bestBag->getBagLocalSearch()) << std::endl;
    outFile << "Benefit Value: " << bestBag->getBenefit() << std::endl;
    outFile << "Total Disk Space (Dependencies): " << bestBag->getSize() << " MB" << std::endl;
    outFile << "Solution (Binary Vectors):" << std::endl;

    // --- Packages Vector ---
    std::unordered_set<std::string> solutionPackages;
    for (const auto& pkg : bestBag->getPackages()) {
        solutionPackages.insert(pkg->getName());
    }

    outFile << "[";
    // CORRECTED: Iterate numerically to ensure correct order ("0", "1", "2", ... "10")
    for (size_t i = 0; i < allPackages.size(); ++i) {
        std::string name = std::to_string(i);
        bool found = solutionPackages.count(name);
        outFile << (found ? "1" : "0") << (i == allPackages.size() - 1 ? "" : ", ");
    }
    outFile << "]" << std::endl;

    // --- Dependencies Vector (Same corrected logic) ---
    std::unordered_set<std::string> solutionDependencies;
    for (const auto& dep : bestBag->getDependencies()) {
        solutionDependencies.insert(dep->getName());
    }
    
    outFile << "[";
    // CORRECTED: Iterate numerically
    for (size_t i = 0; i < allDependencies.size(); ++i) {
        std::string name = std::to_string(i);
        bool found = solutionDependencies.count(name);
        outFile << (found ? "1" : "0") << (i == allDependencies.size() - 1 ? "" : ", ");
    }
    outFile << "]" << std::endl;

    outFile << "Metaheuristic Parameters: " << (bestBag->getMetaheuristicParameters().empty() ? "N/A" : bestBag->getMetaheuristicParameters()) << std::endl;
    outFile << "Random Seed: " << seed << std::endl;
    outFile << "Execution Time: " << std::fixed << std::setprecision(5) << bestBag->getAlgorithmTime() << " seconds" << std::endl;

    outFile.close();
    std::cout << "\nDetailed report saved to " << reportFileName << std::endl;
}

std::string formatTimestampForFilename(std::string timestamp) {
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');
    std::replace(timestamp.begin(), timestamp.end(), ':', '_');
    return timestamp;
}