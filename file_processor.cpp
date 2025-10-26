#include "file_processor.h"

// Project-specific headers
#include "search_engine.h"
#include "solution_repair.h"
#include "algorithm.h"

// Standard library headers
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

namespace FILE_PROCESSOR {

// ────────────────────────────────────────────────
// CSV summary saver for single Bag
// ────────────────────────────────────────────────
void saveData(const std::unique_ptr<Bag>& bag,
              const std::string& outputDir,
              const std::string& inputFilename,
              const std::string& fileId)
{
    if (!bag || outputDir.empty()) {
        std::cerr << "Error: Bag is null or output directory is empty.\n";
        return;
    }

    // --- Safe timestamp for folder ---
    std::string bagTimestampSafe = FILE_PROCESSOR::formatTimestampForFileName(bag->getTimestamp());

    // --- Folder path ---
    std::filesystem::path folderPath = std::filesystem::path(outputDir) / ("reports-" + bagTimestampSafe);

    // Create folder if it does not exist
    std::error_code ec;
    if (!std::filesystem::exists(folderPath)) {
        if (!std::filesystem::create_directories(folderPath, ec)) {
            std::cerr << "Error: Could not create folder " << folderPath.string()
                      << " (" << ec.message() << ")" << std::endl;
            return;
        }
    }

    // --- CSV file path ---
    std::filesystem::path csvPath = folderPath / ("summary_results-" + bagTimestampSafe + ".csv");

    // Check if file exists
    bool writeHeader = !std::filesystem::exists(csvPath) || std::filesystem::file_size(csvPath) == 0;

    std::ofstream outFile(csvPath, std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open CSV file " << csvPath.string() << std::endl;
        return;
    }

    std::string sep = ",";

    if (writeHeader) {
        outFile << "Algorithm" << sep
                << "Movement" << sep
                << "Feasibility Strategy" << sep
                << "File name" << sep
                << "Timestamp" << sep
                << "Processing Time (h:m:s.ms)" << sep
                << "Packages" << sep
                << "Dependencies" << sep
                << "Bag Weight" << sep
                << "Bag Benefit" << sep
                << "Seed" << sep
                << "Metaheuristic Parameters" << "\n";
    }

    std::string algStr = ALGORITHM::toString(bag->getBagAlgorithm());
    std::string locStr = ALGORITHM::toString(bag->getBagLocalSearch());
    if (locStr != "NONE") algStr += " | " + locStr;

    outFile << algStr << sep
            << SEARCH_ENGINE::toString(bag->getMovementType()) << sep
            << SOLUTION_REPAIR::toString(bag->getFeasibilityStrategy()) << sep
            << inputFilename + "-" + fileId << sep
            << bag->getTimestamp() << sep
            << bag->getAlgorithmTimeString() << sep
            << bag->getPackages().size() << sep
            << bag->getDependencies().size() << sep
            << bag->getSize() << sep
            << bag->getBenefit() << sep
            << bag->getSeed() << sep
            << "\"" << bag->getMetaheuristicParameters() << "\""
            << "\n";

    outFile.close();
}


// ────────────────────────────────────────────────
// Detailed report saver for single Bag
// ────────────────────────────────────────────────
std::string saveReport(const std::unique_ptr<Bag>& bag,
                       const std::vector<Package*>& allPackages,
                       const std::vector<Dependency*>& allDependencies,
                       const std::string& timestamp,
                       const std::string& outputDir,
                       const std::string& inputFilename,
                       const std::string& fileId)
{
    if (!bag || outputDir.empty()) {
        std::cerr << "Error: Bag is null or output directory is empty.\n";
        return "";
    }

    // --- Safe timestamp for folder ---
    std::string bagTimestampSafe = FILE_PROCESSOR::formatTimestampForFileName(bag->getTimestamp());

    // --- Folder path ---
    std::filesystem::path folderPath = std::filesystem::path(outputDir) / ("reports-" + bagTimestampSafe);

    // Create folder if it does not exist
    std::error_code ec;
    if (!std::filesystem::exists(folderPath)) {
        if (!std::filesystem::create_directories(folderPath, ec)) {
            std::cerr << "Error: Could not create folder " << folderPath.string()
                      << " (" << ec.message() << ")" << std::endl;
            return "";
        }
    }

    // --- Report file path ---
    std::filesystem::path reportFile = folderPath / ("report_" 
                                                     + std::to_string(bag->getBenefit()) + "-" 
                                                     + ALGORITHM::toString(bag->getBagAlgorithm()) + "-"
                                                     + SEARCH_ENGINE::toString(bag->getMovementType()) + "-"
                                                     + FILE_PROCESSOR::formatTimestampForFileName(fileId) 
                                                     + ".txt");

    std::ofstream outFile(reportFile);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open report file " << reportFile.string() << std::endl;
        return "";
    }

    // --- Write detailed report ---
    outFile << "=== BAG REPORT ===\n";
    outFile << "Algorithm: " << ALGORITHM::toString(bag->getBagAlgorithm()) << "\n";
    outFile << "Local Search: " << ALGORITHM::toString(bag->getBagLocalSearch()) << "\n";
    outFile << "Movement: " << SEARCH_ENGINE::toString(bag->getMovementType()) << "\n";
    outFile << "Feasibility Strategy: " << SOLUTION_REPAIR::toString(bag->getFeasibilityStrategy()) << "\n";
    outFile << "Timestamp: " << bag->getTimestamp() << "\n";
    outFile << "Input File: " << inputFilename + "-" + fileId << "\n";
    outFile << "Processing Time (s): " << bag->getAlgorithmTime() << "\n";
    outFile << "Packages: " << bag->getPackages().size() << "\n";
    outFile << "Dependencies: " << bag->getDependencies().size() << "\n";
    outFile << "Bag Weight: " << bag->getSize() << "\n";
    outFile << "Bag Benefit: " << bag->getBenefit() << "\n";
    outFile << "Seed: " << bag->getSeed() << "\n";
    outFile << "Metaheuristic Parameters: " << bag->getMetaheuristicParameters() << "\n";

    // --- MODIFIED SECTION: PACKAGES ---
    // Create a set of selected package names for efficient lookup
    std::unordered_set<std::string> selectedPackages;
    for (const Package* p : bag->getPackages()) {
        if (p) selectedPackages.insert(p->getName());
    }

    outFile << "\n=== PACKAGES ===\n";
    std::stringstream ssPackages;
    ssPackages << "[";
    bool firstPkg = true;
    for (const Package* p_all : allPackages) {
        if (!firstPkg) {
            ssPackages << ",";
        }
        if (p_all && selectedPackages.count(p_all->getName())) {
            ssPackages << "1";
        } else {
            ssPackages << "0";
        }
        firstPkg = false;
    }
    ssPackages << "]";
    outFile << ssPackages.str() << "\n";
    // --- END MODIFIED SECTION ---


    // --- MODIFIED SECTION: DEPENDENCIES ---
    // Create a set of selected dependency names for efficient lookup
    std::unordered_set<std::string> selectedDependencies;
    for (const Dependency* d : bag->getDependencies()) {
        if (d) selectedDependencies.insert(d->getName());
    }

    outFile << "\n=== DEPENDENCIES ===\n";
    std::stringstream ssDependencies;
    ssDependencies << "[";
    bool firstDep = true;
    for (const Dependency* d_all : allDependencies) {
        if (!firstDep) {
            ssDependencies << ",";
        }
        if (d_all && selectedDependencies.count(d_all->getName())) {
            ssDependencies << "1";
        } else {
            ssDependencies << "0";
        }
        firstDep = false;
    }
    ssDependencies << "]";
    outFile << ssDependencies.str() << "\n";
    // --- END MODIFIED SECTION ---

    outFile.close();
    return reportFile.string();
}

// ----------------------
// Load problem
// ----------------------
/**
 * @brief Loads a problem instance from a .knapsack format file.
 *
 * File Format Expected:
 * 1. Header: <num_packages> <num_dependencies> <num_pairs> <max_capacity>
 * 2. Benefits: <p_benefit_0> <p_benefit_1> ... (space-separated)
 * 3. Sizes: <d_size_0> <d_size_1> ... (space-separated)
 * 4. Links: <pkg_index> <dep_index> (one per line)
 * 5. End: }
 */
ProblemInstance loadProblem(const std::string& filename) {
    ProblemInstance problem;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open problem file: " + filename);
    }

    std::string line;
    std::stringstream ss;

    int numPackages = 0;
    int numDependencies = 0;
    int numPairs = 0; // This will read the 'pairs' value

    // --- 1. Read Header ---
    if (std::getline(file, line)) {
        ss.str(line);
        // Reads: num_packages num_dependencies num_pairs max_capacity
        ss >> numPackages >> numDependencies >> numPairs >> problem.maxCapacity;
        ss.clear();
    } else {
        throw std::runtime_error("Error: Cannot read header from file: " + filename);
    }

    if (numPackages <= 0 || numDependencies <= 0) {
        throw std::runtime_error("Error: Invalid package or dependency count in file: " + filename);
    }

    // Reserve space for efficiency
    problem.packages.reserve(numPackages);
    problem.dependencies.reserve(numDependencies);

    // --- 2. Read Package Benefits & Create Packages ---
    if (std::getline(file, line)) {
        ss.str(line);
        int benefit;
        for (int i = 0; i < numPackages; ++i) {
            if (!(ss >> benefit)) {
                throw std::runtime_error("Error: Mismatch in package benefit count. Expected " + std::to_string(numPackages));
            }
            std::string pkgName = "P" + std::to_string(i);
            // 'new' is correct here; ProblemInstance destructor will manage memory
            problem.packages.push_back(new Package(pkgName, benefit));
        }
        ss.clear();
    } else {
        throw std::runtime_error("Error: Cannot read package benefits from file: " + filename);
    }

    // --- 3. Read Dependency Sizes & Create Dependencies ---
    if (std::getline(file, line)) {
        ss.str(line);
        int size;
        for (int i = 0; i < numDependencies; ++i) {
            if (!(ss >> size)) {
                throw std::runtime_error("Error: Mismatch in dependency size count. Expected " + std::to_string(numDependencies));
            }
            std::string depName = "D" + std::to_string(i);
            // 'new' is correct here; ProblemInstance destructor will manage memory
            problem.dependencies.push_back(new Dependency(depName, size));
        }
        ss.clear();
    } else {
        throw std::runtime_error("Error: Cannot read dependency sizes from file: " + filename);
    }

    // --- 4. Read and Link Dependencies ---
    int packageIndex, dependencyIndex;
    while (std::getline(file, line)) {
        // Stop if we hit the end-of-file marker or an empty line
        if (line.empty() || line[0] == '}') {
            continue; // Skip empty lines or the final brace
        }

        // Ignore the tags
        if (line[0] == '[') {
            continue;
        }

        ss.str(line);
        if (ss >> packageIndex >> dependencyIndex) {
            // Bounds checking
            if (packageIndex >= 0 && packageIndex < numPackages &&
                dependencyIndex >= 0 && dependencyIndex < numDependencies) {
                
                Package* pkg = problem.packages[packageIndex];
                Dependency* dep = problem.dependencies[dependencyIndex];

                // Link them in both directions
                pkg->addDependency(*dep);
                dep->addAssociatedPackage(pkg);

            } else {
                std::cerr << "Warning: Out-of-bounds index in " << filename << ": " << line << std::endl;
            }
        } else {
            // This case might be triggered by a line with just one number
            // or a malformed line. We can just ignore it as it's not a valid pair.
            // std::cerr << "Warning: Skipping malformed line in " << filename << ": " << line << std::endl;
        }
        ss.clear();
    }

    return problem;
}


// ----------------------
// Load solution report
// ----------------------
SolutionReport loadReport(const std::string& filename) {
    SolutionReport report;
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Cannot open report file: " + filename);

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // Parse reported benefit
        if (line.find("Bag Benefit:") != std::string::npos) {
            report.reportedBenefit = std::stol(line.substr(line.find(":") + 1));
        }
        // Parse reported weight
        else if (line.find("Bag Weight:") != std::string::npos) {
            report.reportedWeight = std::stol(line.substr(line.find(":") + 1));
        }
        // --- MODIFIED SECTION: PACKAGES ---
        // Parse packages from binary vector [0,0,1,...]
        else if (line.find("=== PACKAGES ===") != std::string::npos) {
            if (std::getline(file, line) && !line.empty()) { // Read the binary string line [0,0,1,...]
                int index = 0;
                for (char c : line) {
                    if (c == '1') {
                        report.packageVector.push_back(index);
                        index++;
                    } else if (c == '0') {
                        index++;
                    }
                    // Ignore '[' ']' and ','
                }
            }
        }
        // --- END MODIFIED SECTION ---

        // --- MODIFIED SECTION: DEPENDENCIES ---
        // Parse dependencies from binary vector [0,0,1,...]
        else if (line.find("=== DEPENDENCIES ===") != std::string::npos) {
            if (std::getline(file, line) && !line.empty()) { // Read the binary string line [0,0,1,...]
                int index = 0;
                for (char c : line) {
                    if (c == '1') {
                        report.dependencyVector.push_back(index);
                        index++;
                    } else if (c == '0') {
                        index++;
                    }
                    // Ignore '[' ']' and ','
                }
            }
        }
        // --- END MODIFIED SECTION ---
    }

    return report;
}

// ----------------------
// Validate solution
// ----------------------
ValidationResult validateSolution(const std::string& problemFilename,
                                  const std::string& reportFilename) {
    ProblemInstance problem = loadProblem(problemFilename);
    problem.buildDependencyMap(); // build name->Dependency* map
    SolutionReport report = loadReport(reportFilename);

    ValidationResult result;
    std::unordered_set<std::string> usedDependencies;

    // --- Validate packages ---
    result.packageCount = static_cast<int>(report.packageVector.size());
    result.calculatedBenefit = 0;
    result.trueWeight = 0;

    for (int pkgIdx : report.packageVector) {
        if (pkgIdx >= 0 && pkgIdx < static_cast<int>(problem.packages.size())) {
            Package* pkg = problem.packages[pkgIdx];
            result.calculatedBenefit += pkg->getBenefit();

            // Collect dependencies by name
            for (const auto& dep : pkg->getDependencies()) {
                usedDependencies.insert(dep.first);
            }
        } else {
            std::cerr << "Warning: Package index " << pkgIdx << " not found in problem instance\n";
        }
    }

    // --- Validate dependencies from report ---
    result.reportedDependencyCount = static_cast<int>(report.dependencyVector.size());
    for (int depIdx : report.dependencyVector) {
        if (depIdx >= 0 && depIdx < static_cast<int>(problem.dependencies.size())) {
            Dependency* d = problem.dependencies[depIdx];
            result.trueWeight += d->getSize();

            if (usedDependencies.find(d->getName()) == usedDependencies.end()) {
                std::cerr << "Warning: Reported dependency " << d->getName()
                          << " not actually used by selected packages\n";
            }
        } else {
            std::cerr << "Warning: Dependency index " << depIdx
                      << " not found in problem instance\n";
        }
    }

    result.trueRequiredDependencyCount = static_cast<int>(usedDependencies.size());

    // --- Validate weight and benefit ---
    result.isReportedWeightValid = (report.reportedWeight <= problem.maxCapacity);
    result.isBenefitValid = (report.reportedBenefit == result.calculatedBenefit);

    // --- Consistency: reported packages vs dependencies ---
    result.isConsistent = (usedDependencies.size() == report.dependencyVector.size());
    result.isFeasible = (report.reportedWeight <= problem.maxCapacity);

    return result;
}

// -------------------------------------------------------------------
// Helper Functions
// -------------------------------------------------------------------

// ----------------------
// Convert backslashes to slashes
// ----------------------
std::string backslashesPathToSlashesPath(const std::string& s) {
    std::string result = s;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

// ----------------------
// Format timestamp for filename
// ----------------------
std::string formatTimestampForFileName(const std::string& ts) {
    std::string formatted = ts;

    // Replace spaces with underscore
    std::replace(formatted.begin(), formatted.end(), ' ', '_');

    // Replace colons with dash
    std::replace(formatted.begin(), formatted.end(), ':', '-');

    // Replace dots in milliseconds with dash (if any)
    std::replace(formatted.begin(), formatted.end(), '.', '-');

    // Optional: remove any other unsafe characters (just in case)
    formatted.erase(std::remove_if(formatted.begin(), formatted.end(),
        [](char c) { return !(isalnum(c) || c == '-' || c == '_'); }),
        formatted.end());

    return formatted;
}

// ----------------------
// Create unique output directory
// ----------------------
std::string createUniqueOutputDir(const std::string& base) {
    namespace fs = std::filesystem;
    fs::path dir(base);
    int counter = 0;

    while (fs::exists(dir)) {
        ++counter;
        dir = base + "-" + std::to_string(counter);
    }

    fs::create_directories(dir);
    return dir.string();
}

} // namespace FILE_PROCESSOR