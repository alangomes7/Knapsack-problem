#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <iomanip>
#include <string_view>

#include "fileprocessor.h"

/**
 * @brief Simple helper function to split a string by a delimiter.
 */
std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str); // Corrected: istringstream is a type, not a function.
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

/**
 * @brief Parses the [1, 0, 1, ...] vector strings from the report.
 */
std::vector<int> parseVector(const std::string& line) {
    std::vector<int> vec;
    auto first = line.find('[');
    auto last = line.find_last_of(']');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return vec;

    std::string content = line.substr(first + 1, last - first - 1);
    auto tokenStream = std::istringstream(content);  // ✅ safe initialization
    std::string token;

    while (std::getline(tokenStream, token, ',')) {
        auto start = token.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = token.find_last_not_of(" \t");
        vec.push_back(std::stoi(token.substr(start, end - start + 1)));
    }
    return vec;
}

/**
 * @brief Holds the data from the problem instance file.
 */
struct ProblemInstance {
    int maxCapacity = 0;
    std::vector<int> packageBenefits;
    std::vector<int> dependencyWeights;
    std::unordered_map<int, std::vector<int>> packageToDeps;
};

/**
 * @brief Holds the data from the solution report file.
 */
struct SolutionReport {
    long reportedBenefit = 0;
    long reportedWeight = 0;
    std::vector<int> packageVector;
    std::vector<int> dependencyVector;
};

/**
 * @brief Loads and parses the problem definition from a .knapsack.txt file.
 */
ProblemInstance loadProblem(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open problem file: " + filename);
    }

    ProblemInstance instance;
    std::string line;

    // Line 1: m n ne b
    std::getline(file, line);
    std::stringstream ss_header(line);
    int m, n, ne;
    ss_header >> m >> n >> ne >> instance.maxCapacity;

    // Line 2: Package Benefits
    std::getline(file, line);
    std::stringstream ss_benefits(line);
    int benefit;
    while (ss_benefits >> benefit) {
        instance.packageBenefits.push_back(benefit);
    }

    // Line 3: Dependency Weights
    std::getline(file, line);
    std::stringstream ss_weights(line);
    int weight;
    while (ss_weights >> weight) {
        instance.dependencyWeights.push_back(weight);
    }

    // Lines 4+: Package-Dependency pairs
    while (std::getline(file, line)) {
        std::stringstream ss_pair(line);
        int packageIndex, depIndex;
        if (ss_pair >> packageIndex >> depIndex) {
            instance.packageToDeps[packageIndex].push_back(depIndex);
        }
    }

    std::cout << "-> Problem file loaded: " << filename << "\n";
    std::cout << "   Max Capacity: " << instance.maxCapacity << " MB\n";
    std::cout << "   Packages:     " << instance.packageBenefits.size() << "\n";
    std::cout << "   Dependencies: " << instance.dependencyWeights.size() << "\n\n";
    return instance;
}

/**
 * @brief Loads and parses the solution from a report.txt file.
 */
SolutionReport loadReport(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Error: Could not open report file: " + filename);
    }

    SolutionReport report;
    std::string line;

    while (std::getline(file, line)) {
        // Use C++20 'starts_with' for cleaner parsing
        if (line.starts_with("Benefit Value: ")) {
            report.reportedBenefit = std::stol(line.substr(15));
        } else if (line.starts_with("Total Disk Space (Dependencies): ")) {
            report.reportedWeight = std::stol(line.substr(33));
        } else if (line.starts_with("[")) {
            // Simple heuristic to distinguish the vectors based on the first few elements
            if (line.starts_with("[1, 0,") || line.starts_with("[0, 0,")) {
                report.packageVector = parseVector(line);
            } else if (line.starts_with("[0, 1,") || line.starts_with("[1, 1,")) {
                report.dependencyVector = parseVector(line);
            }
        }
    }
    
    std::cout << "-> Report file loaded: " << filename << "\n";
    std::cout << "   Reported Benefit: " << report.reportedBenefit << "\n";
    std::cout << "   Reported Weight:  " << report.reportedWeight << " MB\n\n";
    return report;
}

/**
 * @brief Helper to print validation results in a clean table.
 */
void printResult(const std::string& item, long reportValue, long calcValue, bool valid) {
    std::cout << std::left << std::setw(20) << item
              << std::left << std::setw(18) << reportValue
              << std::left << std::setw(18) << calcValue
              << (valid ? "✅ Valid" : "🚫 INVALID") << "\n";
}

int main() {
    try {
        std::string problemFile = "C:/Users/alang/OneDrive/Documents/code/Knapsack-problem/input/sukp02_100_85_0.10_0.75.knapsack.txt";
        std::string reportFile  = "C:/Users/alang/OneDrive/Documents/code/Knapsack-problem/input/output-2/report_sukp02_100_85_0.10_0.75.knapsack_2025-10-17_12_26_34.txt";

        // -- 0. Fix the file paths -- //
        FileProcessor fileProcessor("");
        problemFile = fileProcessor.backslashesPathToSlashesPath(problemFile);
        reportFile = fileProcessor.backslashesPathToSlashesPath(reportFile);
        // --- 1. LOAD FILES ---
        ProblemInstance problem = loadProblem(problemFile);
        SolutionReport report = loadReport(reportFile);

        std::cout << "--- 3. Validation Steps ---" << "\n";
        bool isConsistent = true;
        bool isFeasible = true;

        // --- 2. VALIDATE PACKAGE BENEFIT ---
        long calculatedBenefit = 0;
        int packageCount = 0;
        for (size_t i = 0; i < report.packageVector.size(); ++i) {
            if (report.packageVector[i] == 1) {
                calculatedBenefit += problem.packageBenefits[i];
                packageCount++;
            }
        }

        // --- 3. VALIDATE DEPENDENCY WEIGHT ---
        long calculatedReportWeight = 0;
        int depCount = 0;
        for (size_t i = 0; i < report.dependencyVector.size(); ++i) {
            if (report.dependencyVector[i] == 1) {
                calculatedReportWeight += problem.dependencyWeights[i];
                depCount++;
            }
        }

        // --- 4. VALIDATE FEASIBILITY & CONSISTENCY ---
        std::unordered_set<int> requiredDeps;
        for (size_t i = 0; i < report.packageVector.size(); ++i) {
            if (report.packageVector[i] == 1) {
                // Find all dependencies for this package
                auto it = problem.packageToDeps.find(i);
                if (it != problem.packageToDeps.end()) {
                    // Add them to the set (handles duplicates)
                    requiredDeps.insert(it->second.begin(), it->second.end());
                }
            }
        }
        
        // Calculate true weight from the *required* dependencies
        long trueWeight = 0;
        for (int depIndex : requiredDeps) {
            trueWeight += problem.dependencyWeights[depIndex];
        }

        // Check consistency
        if (requiredDeps.size() != depCount) {
            isConsistent = false;
        }

        // Check feasibility
        if (trueWeight > problem.maxCapacity) {
            isFeasible = false;
        }

        // --- 5. PRINT REPORT ---
        std::cout << std::left << std::setw(20) << "Item"
                  << std::left << std::setw(18) << "Report Value"
                  << std::left << std::setw(18) << "Calculated Value"
                  << "Status" << "\n";
        std::cout << std::string(70, '-') << "\n";

        printResult("Bag Benefit", report.reportedBenefit, calculatedBenefit, report.reportedBenefit == calculatedBenefit);
        printResult("Package Count", packageCount, packageCount, true);
        printResult("Bag Weight", report.reportedWeight, calculatedReportWeight, report.reportedWeight == calculatedReportWeight);
        printResult("Dependency Count", depCount, (long)requiredDeps.size(), isConsistent);

        std::cout << "\n--- 4. Final Feasibility Check ---" << "\n";
        std::cout << std::left << std::setw(20) << "Item"
                  << std::left << std::setw(18) << "Value"
                  << std::left << std::setw(18) << "Limit"
                  << "Status" << "\n";
        std::cout << std::string(70, '-') << "\n";
        
        std::cout << std::left << std::setw(20) << "True Weight"
                  << std::left << std::setw(18) << trueWeight
                  << std::left << std::setw(18) << problem.maxCapacity
                  << (isFeasible ? "✅ Feasible" : "🚫 INFEASIBLE") << "\n\n";

        std::cout << "=================================\n";
        std::cout << "  FINAL VALIDATION: " << (isConsistent && isFeasible ? "VALID ✅" : "INVALID 🚫") << "\n";
        std::cout << "=================================\n";


    } catch (const std::exception& e) {
        std::cerr << "Validation failed with an error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}