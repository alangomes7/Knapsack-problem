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
// CSV summary saver (updated to pull data from Bag)
// ────────────────────────────────────────────────
void saveData(const std::vector<std::unique_ptr<Bag>>& bags,
              const std::string& outputDir,
              const std::string& inputFilename,
              const std::string& fileId)
{
    if (bags.empty() || outputDir.empty()) return;

    const std::string csvFile = outputDir + "/summary_results-" + formatTimestampForFileName(fileId) + ".csv";

    bool writeHeader = false;
    {
        std::ifstream inFile(csvFile);
        if (!inFile.is_open() || inFile.peek() == std::ifstream::traits_type::eof())
        {
            writeHeader = true;
        }
    }

    std::ofstream outFile(csvFile, std::ios::app);
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not create or open " << csvFile << std::endl;
        return;
    }

    std::string separator = ",";

    if (writeHeader) {
        outFile << "Algorithm" << separator
                << "Movement" << separator
                << "Feasibility Strategy" << separator
                << "File name" << separator
                << "Timestamp" << separator
                << "Processing Time (h:m:s.ms)" << separator
                << "Packages" << separator
                << "Dependencies" << separator
                << "Bag Weight" << separator
                << "Bag Benefit" << separator
                << "Seed" << separator << "\n";
    }

    for (const std::unique_ptr<Bag>& bag : bags) {
        if (!bag) continue;

        std::string algStr = ALGORITHM::toString(bag->getBagAlgorithm());
        std::string locStr = ALGORITHM::toString(bag->getBagLocalSearch());
        if (locStr != "NONE") algStr += " | " + locStr;

        outFile << algStr << separator
                << SEARCH_ENGINE::toString(bag->getMovementType()) << separator
                << SOLUTION_REPAIR::toString(bag->getFeasibilityStrategy()) << separator
                << inputFilename + "-" + fileId << separator
                << bag->getTimestamp() << separator
                << bag->getAlgorithmTimeString() << separator
                << bag->getPackages().size() << separator
                << bag->getDependencies().size() << separator
                << bag->getSize() << separator
                << bag->getBenefit() << separator
                << bag->getSeed() << separator << "\n";
    }
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
    if (!bag) return "";

    // Ensure timestamp is set
    std::string bagTimestamp = bag->getTimestamp();
    if (bagTimestamp.empty() || bagTimestamp == "0000-00-00 00:00:00") {
        bagTimestamp = timestamp;
    }

    std::string reportFile = outputDir + "/report_" + fileId + "_" + FILE_PROCESSOR::formatTimestampForFileName(bagTimestamp) + ".txt";

    std::ofstream out(reportFile);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open report file " << reportFile << std::endl;
        return "";
    }

    out << "=== BAG REPORT ===\n";
    out << "Algorithm: " << ALGORITHM::toString(bag->getBagAlgorithm()) << "\n";
    out << "Local Search: " << ALGORITHM::toString(bag->getBagLocalSearch()) << "\n";
    out << "Movement: " << SEARCH_ENGINE::toString(bag->getMovementType()) << "\n";
    out << "Feasibility Strategy: " << SOLUTION_REPAIR::toString(bag->getFeasibilityStrategy()) << "\n";
    out << "Timestamp: " << bagTimestamp << "\n";
    out << "Input File: " << inputFilename + "-" + fileId << "\n";
    out << "Processing Time (s): " << bag->getAlgorithmTime() << "\n";
    out << "Packages: " << bag->getPackages().size() << "\n";
    out << "Dependencies: " << bag->getDependencies().size() << "\n";
    out << "Bag Weight: " << bag->getSize() << "\n";
    out << "Bag Benefit: " << bag->getBenefit() << "\n";
    out << "Seed: " << bag->getSeed() << "\n";  // <- pass seed explicitly
    out << "Metaheuristic Parameters: " << bag->getMetaheuristicParameters() << "\n";

    out << "\n=== PACKAGES ===\n";
    for (const Package* p : bag->getPackages()) {
        if (p) {
            out << p->getName() << " (Benefit: " << p->getBenefit() << ")\n";
        }
    }

    out << "\n=== DEPENDENCIES ===\n";
    for (const Dependency* d : bag->getDependencies()) {
        if (d) {
            out << d->getName() << " (Weight: " << d->getSize() << ")\n";
        }
    }

    out.close();
    return reportFile;
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
// Load report
// ----------------------
SolutionReport loadReport(const std::string& filename) {
    SolutionReport report;
    std::ifstream file(filename);
    if (!file.is_open()) throw std::runtime_error("Cannot open report file: " + filename);

    std::string line;
    while (std::getline(file, line)) {
        // Parse report line to fill SolutionReport
        // Simplified: just add to lines for now
        // TODO: Implement actual report parsing
        // report.lines.push_back(line); // Example: this line is not in the original, but shows intent
    }

    return report;
}

// ----------------------
// Validate solution
// ----------------------
std::string validateSolution(const std::string& problemFilename, const std::string& reportFilename) {
    // 'problem' is an RAII object. It will automatically clean up
    // its heap-allocated packages and dependencies when it goes out of scope.
    ProblemInstance problem = loadProblem(problemFilename);
    SolutionReport report = loadReport(reportFilename);

    std::ostringstream msg;
    msg << "Validation report for " << reportFilename << "\n";

    // TODO: Implement full validation logic as described in the .h file
    // (e.g., check dependencies, capacity, and benefits).
    
    // Example: check if package count matches (simplified)
    if (report.packageVector.size() != problem.packages.size()) { // Using packageVector as in SolutionReport struct
        msg << "Mismatch: number of lines in report vs packages\n";
    } else {
        msg << "Number of packages matches.\n";
    }
    
    // **FIXED:** Removed manual deletion.
    // The 'problem' object's destructor will be called automatically
    // when this function returns, correctly freeing the memory.

    return msg.str();
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
    std::replace(formatted.begin(), formatted.end(), ' ', '_');
    std::replace(formatted.begin(), formatted.end(), ':', '-');
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