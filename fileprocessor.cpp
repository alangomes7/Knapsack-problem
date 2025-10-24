#include "fileprocessor.h"

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
//#include "algorithm.h"

namespace FileProcessor {

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
// Everything else stays inside FileProcessor
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

void saveData(const std::vector<std::unique_ptr<Bag>>& bags,
            const std::string& outputDir,
            const std::string& inputFilename,
            const std::string& fileId)
{
    if (bags.empty() || outputDir.empty()) return;

    const std::string csvFile = outputDir + "/summary_results-" + formatTimestampForFilename(fileId) + ".csv";

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

    Algorithm algHelper(0, 0, "", "", "");
    for (const std::unique_ptr<Bag>& bag : bags) {
        if (!bag) continue;

        std::string algStr = algHelper.toString(bag->getBagAlgorithm());
        std::string locStr = algHelper.toString(bag->getBagLocalSearch());
        if (locStr != "None") algStr += " | " + locStr;

        outFile << algStr << separator
                << bag->toString(bag->getMovementType()) << separator
                << bag->toString(bag->getFeasibilityStrategy()) << separator
                << inputFilename << separator
                << bag->getTimestamp() << separator
                << bag->getAlgorithmTimeString() << separator
                << bag->getPackages().size() << separator
                << bag->getDependencies().size() << separator
                << bag->getSize() << separator
                << bag->getBenefit() << "\n";
    }
}

void saveReport(const std::vector<Bag*>& bags,
                const std::vector<Package*>& allPackages,
                const std::vector<Dependency*>& allDependencies,
                unsigned int seed,
                const std::string& timestamp,
                const std::string& outputDir,
                const std::string& inputFilename)
{
    if (bags.empty() || outputDir.empty()) {
        std::cerr << "Error: Cannot generate report from empty set." << std::endl;
        return;
    }

    const Bag* bestBag = *std::max_element(bags.begin(), bags.end(),
        [](const Bag* a, const Bag* b){ return a->getBenefit() < b->getBenefit(); });

    std::filesystem::path inputPath(inputFilename);
    std::string stem = inputPath.stem().string();
    std::string reportFileName = outputDir + "/report_" + stem + "_" +
                                 formatTimestampForFilename(timestamp) + ".txt";

    std::ofstream out(reportFileName);
    if (!out.is_open()) {
        std::cerr << "Error: Could not open " << reportFileName << "\n";
        return;
    }

    //Algorithm algHelper(0,0);
    std::string algStr = "algHelper.toString(bestBag->getBagAlgorithm())";
    std::string locStr = "algHelper.toString(bestBag->getBagLocalSearch())";
    if (locStr != "None") algStr += " | " + locStr;

    out << "--- Experiment Reproduction Report for " << inputFilename << " ---\n";
    out << "Metaheuristic: " << algStr << "\n";
    out << "Benefit Value: " << bestBag->getBenefit() << "\n";
    out << "Total Disk Space (Dependencies): " << bestBag->getSize() << " MB\n";

    // Print binary vectors
    std::unordered_set<std::string> packSet, depSet;
    for (auto* p : bestBag->getPackages()) packSet.insert(p->getName());
    for (auto* d : bestBag->getDependencies()) depSet.insert(d->getName());

    std::vector<std::string> pkgNames, depNames;
    for (auto* p : allPackages) pkgNames.push_back(p->getName());
    for (auto* d : allDependencies) depNames.push_back(d->getName());
    std::sort(pkgNames.begin(), pkgNames.end());
    std::sort(depNames.begin(), depNames.end());

    out << "[";
    for (size_t i=0;i<pkgNames.size();++i)
        out << (packSet.count(pkgNames[i]) ? "1" : "0")
            << (i+1<pkgNames.size()?", ":"");
    out << "]\n[";

    for (size_t i=0;i<depNames.size();++i)
        out << (depSet.count(depNames[i]) ? "1" : "0")
            << (i+1<depNames.size()?", ":"");
    out << "]\n";

    out << "Metaheuristic Parameters: "
        << (bestBag->getMetaheuristicParameters().empty()
            ? "N/A" : bestBag->getMetaheuristicParameters()) << "\n";
    out << "Random Seed: " << seed << "\n";
    out << "Execution Time: " << std::fixed << std::setprecision(5)
        << bestBag->getAlgorithmTime() << " s\n";
    out << "Timestamp: " << timestamp << "\n";

    std::cout << "Detailed report saved to " << reportFileName << "\n";
}

std::string backslashesPathToSlashesPath(const std::string& s) {
    std::string r=s;
    std::replace(r.begin(), r.end(),'\\','/');
    return r;
}

std::string formatTimestampForFilename(const std::string& ts) {
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

} // namespace FileProcessor
