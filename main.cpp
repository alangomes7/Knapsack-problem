#include "main.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <limits>
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

    // Find the best bag to generate the report
    if (!bags.empty()) {
        auto bestBagIt = std::max_element(bags.begin(), bags.end(), [](const Bag* a, const Bag* b) {
            return a->getBenefit() < b->getBenefit();
        });
        saveReport(*bestBagIt, availablePackages, availableDependencies, seed, filePath, timestamp);
    }

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

void saveReport(const Bag* bestBag, const std::unordered_map<std::string, 
    Package*>& allPackages, const std::unordered_map<std::string, 
    Dependency*>& allDependencies, unsigned int seed, const std::string& filePath, 
    const std::string& timestamp) {
    if (!bestBag) return;

    // --- Create a unique report filename with timestamp ---
    std::string base_filename = filePath.substr(filePath.find_last_of("/\\") + 1);
    // Remove the original extension to avoid names like "file.txt.txt"
    size_t lastindex = base_filename.find_last_of(".");
    std::string raw_name = base_filename.substr(0, lastindex);

    std::string path_directory = filePath.substr(0, filePath.find_last_of("/\\") + 1);
    std::string reportFileName = path_directory + "report_" + raw_name + "_" + formatTimestampForFilename(timestamp) + "knapsack.txt";
    // ---

    std::ofstream outFile(reportFileName);

    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open " << reportFileName << " for writing." << std::endl;
        return;
    }

    outFile << "--- Experiment Reproduction Report for " << base_filename << " ---" << std::endl;

    // Methaeuristic used
    Algorithm algorithmAux(0);
    outFile << "Methaeuristic: " << algorithmAux.toString(bestBag->getBagAlgorithm()) + " | " + algorithmAux.toString(bestBag->getBagLocalSearch()) << std::endl;

    // (a) Benefit value
    outFile << "Benefit Value: " << bestBag->getBenefit() << std::endl;

    // (b) Total disk space
    outFile << "Total Disk Space (Dependencies): " << bestBag->getSize() << " MB" << std::endl;

    // (c) Solution as binary vectors
    outFile << "Solution (Binary Vectors):" << std::endl;
    // Packages vector
    outFile << "[";
    for (int i = 0; i < allPackages.size(); ++i) {
        std::string name = std::to_string(i);
        bool found = false;
        for (const auto& pkg : bestBag->getPackages()) {
            if (pkg->getName() == name) {
                found = true;
                break;
            }
        }
        outFile << (found ? "1" : "0") << (i == allPackages.size() - 1 ? "" : ", ");
    }
    outFile << "]" << std::endl;
    // Dependencies vector
    outFile << "[";
    for (int i = 0; i < allDependencies.size(); ++i) {
        std::string name = std::to_string(i);
         bool found = false;
        for (const auto& dep : bestBag->getDependencies()) {
            if (dep->getName() == name) {
                found = true;
                break;
            }
        }
        outFile << (found ? "1" : "0") << (i == allDependencies.size() - 1 ? "" : ", ");
    }
    outFile << "]" << std::endl;


    // (d) Metaheuristic parameters
    outFile << "Metaheuristic Parameters: " << (bestBag->getMetaheuristicParameters().empty() ? "N/A" : bestBag->getMetaheuristicParameters()) << std::endl;

    // (e) Random seed
    outFile << "Random Seed: " << seed << std::endl;

    // (f) Execution time
    outFile << "Execution Time: " << std::fixed << std::setprecision(5) << bestBag->getAlgorithmTime() << " seconds" << std::endl;

    outFile.close();
    std::cout << "\nDetailed report saved to " << reportFileName << std::endl;
}

std::string formatTimestampForFilename(std::string timestamp) {
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');
    std::replace(timestamp.begin(), timestamp.end(), ':', '_');
    return timestamp;
}