#include "file_processor.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <unordered_set>
#include <stdexcept>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "solution_repair.h"
#include "search_engine.h"

namespace FILE_PROCESSOR {

// ────────────────────────────────────────────────
// Helper: only visible inside this translation unit
// ────────────────────────────────────────────────
static std::vector<int> parseVector(const std::string& line) {
    std::vector<int> vec;

    auto first = line.find('[');
    auto last = line.find_last_of(']');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return vec;

    std::string_view view(line.c_str() + first + 1, last - first - 1);
    std::string token;
    std::istringstream stream{std::string(view)};
    
    while (std::getline(stream, token, ',')) {
        auto start = token.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        auto end = token.find_last_not_of(" \t");
        vec.push_back(std::stoi(token.substr(start, end - start + 1)));
    }

    return vec;
}

// ────────────────────────────────────────────────
// Problem file loader
// ────────────────────────────────────────────────
ProblemInstance loadProblem(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Could not open problem file: " + filename);

    ProblemInstance problemInstance;
    std::string line;

    // 1. Read header line
    std::getline(file, line);
    std::stringstream ss(line);
    int m, n, ne; // m=packages, n=dependencies, ne=edges
    ss >> m >> n >> ne >> problemInstance.maxCapacity;

    // Reserve space for efficiency
    problemInstance.packages.reserve(m);
    problemInstance.dependencies.reserve(n);

    // 2. Read package benefits and create Package objects
    std::getline(file, line);
    std::stringstream sb(line);
    int b;
    int p_idx = 0;
    while (sb >> b) {
        // Assuming Package(std::string name, int benefit) constructor
        problemInstance.packages.push_back(new Package("P" + std::to_string(p_idx++), b));
    }
    if (problemInstance.packages.size() != m) {
        throw std::runtime_error("Package count mismatch in " + filename);
    }

    // 3. Read dependency weights and create Dependency objects
    std::getline(file, line);
    std::stringstream sw(line);
    int w;
    int d_idx = 0;
    while (sw >> w) {
        // Assuming Dependency(std::string name, int weight) constructor
        problemInstance.dependencies.push_back(new Dependency("D" + std::to_string(d_idx++), w));
    }
    if (problemInstance.dependencies.size() != n) {
        throw std::runtime_error("Dependency count mismatch in " + filename);
    }

    // 4. Read edge list and link objects
    // This assumes Package has a method like addDependency(Dependency* dep)
    int p, d;
    for (int i = 0; i < ne; ++i) {
         if (!std::getline(file, line)) {
             throw std::runtime_error("Unexpected end of file reading edges in " + filename);
         }
         std::stringstream sp(line);
         if (sp >> p >> d) {
             // Add boundary checks
             if (p < 0 || p >= m || d < 0 || d >= n) {
                 throw std::runtime_error("Invalid package/dependency index in " + filename);
             }
             // This method MUST exist in your Package class
             problemInstance.packages[p]->addDependency(*problemInstance.dependencies[d]);
         }
    }

    std::cout << "-> Problem file loaded (Object-oriented): " << filename << "\n";
    return problemInstance;
}

// ────────────────────────────────────────────────
// CSV summary saver
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
                << "Bag Benefit" << "\n";
    }

    for (const std::unique_ptr<Bag>& bag : bags) {
        if (!bag) continue;

        std::string algStr = ALGORITHM::toString(bag->getBagAlgorithm());
        std::string locStr = ALGORITHM::toString(bag->getBagLocalSearch());
        if (locStr != "None") algStr += " | " + locStr;

        outFile << algStr << separator
                << SEARCH_ENGINE::toString(bag->getMovementType()) << separator
                << SOLUTION_REPAIR::toString(bag->getFeasibilityStrategy()) << separator
                << inputFilename << separator
                << bag->getTimestamp() << separator
                << bag->getAlgorithmTimeString() << separator
                << bag->getPackages().size() << separator
                << bag->getDependencies().size() << separator
                << bag->getSize() << separator
                << bag->getBenefit() << "\n";
    }
}

// ────────────────────────────────────────────────
// Detailed TXT report saver (CORRECTED)
// ────────────────────────────────────────────────
std::string saveReport(const std::unique_ptr<Bag>& bag,
                       const std::vector<Package*>& allPackages,
                       const std::vector<Dependency*>& allDependencies,
                       unsigned int seed,
                       const std::string& timestamp,
                       const std::string& outputDir,
                       const std::string& inputFilename)
{
    if (!bag) {
        std::string error = "Error: Cannot generate report from empty bag.";
        std::cerr << error << std::endl;
        return error;
    }

    std::filesystem::create_directories(outputDir);

    std::filesystem::path inputPath(inputFilename);
    std::string stem = inputPath.stem().string();
    std::string reportFileName = outputDir + "/report_" + stem + "_" +
                                 formatTimestampForFileName(timestamp) + ".txt";

    std::ofstream out(reportFileName);
    if (!out.is_open()) {
        std::string error = "Error: Could not open " + reportFileName;
        std::cerr << error << "\n";
        return error;
    }

    std::string algStr = ALGORITHM::toString(bag->getBagAlgorithm());
    std::string locStr = ALGORITHM::toString(bag->getBagLocalSearch());
    if (locStr != "None") algStr += " | " + locStr;

    out << "--- Experiment Reproduction Report for " << inputFilename << " ---\n";
    out << "Metaheuristic: " << algStr << "\n";
    out << "Benefit Value: " << bag->getBenefit() << "\n";
    out << "Total Disk Space (Dependencies): " << bag->getSize() << " MB\n";

    // --- Binary selection vectors ---
    std::unordered_set<std::string> selectedPackages, selectedDeps;
    for (const Package* p : bag->getPackages()) selectedPackages.insert(p->getName());
    // Note: requires const Dependency* from Bag::getDependencies()
    for (const Dependency* d : bag->getDependencies()) selectedDeps.insert(d->getName());

    // Package vector
    out << "[";
    for (size_t i = 0; i < allPackages.size(); ++i)
        out << (selectedPackages.count(allPackages[i]->getName()) ? "1" : "0")
            << (i + 1 < allPackages.size() ? ", " : "");
    out << "]\n";

    // Dependency vector
    out << "[";
    for (size_t i = 0; i < allDependencies.size(); ++i)
        out << (selectedDeps.count(allDependencies[i]->getName()) ? "1" : "0")
            << (i + 1 < allDependencies.size() ? ", " : "");
    out << "]\n";

    // Metaheuristic parameters, seed, timestamp, execution time
    out << "Metaheuristic Parameters: "
        << (bag->getMetaheuristicParameters().empty() ? "N/A"
                                                     : bag->getMetaheuristicParameters())
        << "\n";
    out << "Seed: " << seed << "\n";
    out << "Timestamp: " << timestamp << "\n";
    out << "Execution Time: " << bag->getAlgorithmTimeString() << "\n";

    out.close();
    std::cout << "Report saved to " << reportFileName << "\n";
    return reportFileName;
}

// ────────────────────────────────────────────────
// Report file loader
// ────────────────────────────────────────────────
SolutionReport loadReport(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Could not open report file: " + filename);

    SolutionReport rep;
    std::string line;
    while (std::getline(file,line)) {
        if (line.starts_with("Benefit Value: "))
            rep.reportedBenefit = std::stol(line.substr(15));
        else if (line.starts_with("Total Disk Space (Dependencies): "))
            rep.reportedWeight = std::stol(line.substr(33));
        else if (line.starts_with("[")) {
            if (rep.packageVector.empty())
                rep.packageVector = parseVector(line);
            else if (rep.dependencyVector.empty())
                rep.dependencyVector = parseVector(line);
        }
    }
    std::cout<<"-> Report file loaded: "<<filename<<"\n";
    return rep;
}

// ────────────────────────────────────────────────
// Solution Validator (ADDED)
// ────────────────────────────────────────────────
std::string validateSolution(const std::string& problemFilename,
                           const std::string& reportFilename)
{
    ProblemInstance problem;
    SolutionReport report;
    std::string msg; // Used for success or error messages

    // 1. Load both files
    try {
        problem = loadProblem(problemFilename);
        report = loadReport(reportFilename);
    } catch (const std::exception& e) {
        msg = "Validation Error (Load): " + std::string(e.what());
        std::cerr << msg << std::endl;
        return msg;
    }

    // 2. Check for vector size mismatch
    if (problem.packages.size() != report.packageVector.size()) {
        msg = "Validation Failed: Package count mismatch. Problem=" +
              std::to_string(problem.packages.size()) + ", Report=" +
              std::to_string(report.packageVector.size());
        std::cerr << msg << std::endl;
        return msg;
    }
    if (problem.dependencies.size() != report.dependencyVector.size()) {
        msg = "Validation Failed: Dependency count mismatch. Problem=" +
              std::to_string(problem.dependencies.size()) + ", Report=" +
              std::to_string(report.dependencyVector.size());
        std::cerr << msg << std::endl;
        return msg;
    }

    long calculatedBenefit = 0;
    long calculatedWeight = 0;
    // Use pointers as Dependency and Package objects are managed by ProblemInstance
    std::unordered_set<Dependency*> includedDependencies;
    std::unordered_set<Dependency*> requiredDependencies;

    // 3. Calculate actual weight from the report's dependency vector
    //    and build the set of *included* dependencies.
    for (size_t i = 0; i < report.dependencyVector.size(); ++i) {
        if (report.dependencyVector[i] == 1) {
            Dependency* dep = problem.dependencies[i];
            // Assuming Dependency class has getSize() or getWeight()
            calculatedWeight += dep->getSize(); 
            includedDependencies.insert(dep);
        }
    }

    // 4. Calculate actual benefit from the report's package vector
    //    and build the set of *required* dependencies.
    for (size_t i = 0; i < report.packageVector.size(); ++i) {
        if (report.packageVector[i] == 1) {
            Package* pkg = problem.packages[i];
            calculatedBenefit += pkg->getBenefit();
            
            // Assumes Package::getDependencies() returns std::unordered_map<std::string, Dependency *>&
            for (const auto& [depName, reqDep] : pkg->getDependencies()) {
                requiredDependencies.insert(reqDep);
            }
        }
    }

    // --- 5. Run All Validation Checks ---

    // Check A: Correctness of Benefit
    if (calculatedBenefit != report.reportedBenefit) {
        msg = "Validation Failed: Benefit mismatch. Reported=" +
              std::to_string(report.reportedBenefit) + ", Calculated=" +
              std::to_string(calculatedBenefit);
        std::cerr << msg << std::endl;
        return msg;
    }

    // Check B: Correctness of Weight
    if (calculatedWeight != report.reportedWeight) {
        msg = "Validation Failed: Weight mismatch. Reported=" +
              std::to_string(report.reportedWeight) + ", Calculated=" +
              std::to_string(calculatedWeight);
        std::cerr << msg << std::endl;
        return msg;
    }

    // Check C: Feasibility (Capacity)
    if (calculatedWeight > problem.maxCapacity) {
        msg = "Validation Failed: Solution exceeds max capacity. Weight=" +
              std::to_string(calculatedWeight) + ", Max=" +
              std::to_string(problem.maxCapacity);
        std::cerr << msg << std::endl;
        return msg;
    }

    // Check D: Feasibility (Dependencies)
    // All dependencies required by packages MUST be in the solution.
    for (Dependency* reqDep : requiredDependencies) {
        if (includedDependencies.find(reqDep) == includedDependencies.end()) {
            msg = "Validation Failed: Missing required dependency. "
                  "A selected package requires " + reqDep->getName() +
                  " but it is not in the dependency list.";
            std::cerr << msg << std::endl;
            return msg;
        }
    }

    // Check E: Optimality (No unused dependencies)
    // All dependencies in the solution MUST be required by a package.
    if (requiredDependencies.size() != includedDependencies.size()) {
        msg = "Validation Failed: Solution includes unused dependencies. "
              "Required: " + std::to_string(requiredDependencies.size()) +
              ", Included: " + std::to_string(includedDependencies.size());
        std::cerr << msg;
        
        // Find and report the first unused dependency
        for(Dependency* incDep : includedDependencies) {
            if (requiredDependencies.find(incDep) == requiredDependencies.end()) {
                 msg += "\n -> Example unused dependency: " + incDep->getName();
                 std::cerr << "\n -> Example unused dependency: " << incDep->getName() << std::endl;
                 break;
            }
        }
        return msg;
    }

    // If all checks pass:
    msg = "Validation Success: Solution in " + reportFilename +
          " is valid for " + problemFilename + ".";
    std::cout << msg << std::endl;
    return msg; // Return success message
}


// ────────────────────────────────────────────────
// Utility functions
// ────────────────────────────────────────────────
std::string backslashesPathToSlashesPath(const std::string& s) {
    std::string r=s;
    std::replace(r.begin(), r.end(),'\\','/');
    return r;
}

std::string formatTimestampForFileName(const std::string& ts) {
    std::string t=ts;
    std::replace(t.begin(),t.end(),' ', '_');
    std::replace(t.begin(),t.end(),':','-');
    return t;
}

std::string createUniqueOutputDir(const std::string& base) {
    std::string dir=base;
    int c=1;
    while (std::filesystem::exists(dir))
        dir=base + "-" + std::to_string(c++);
    std::filesystem::create_directory(dir);
    return dir;
}

} // namespace FILE_PROCESSOR