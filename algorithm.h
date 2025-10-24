#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>
#include <memory>

#include "DataStructures.h"

class Bag;
class Package;
class Dependency;
class LocalSearch;

/**
 * @brief Provides a collection of algorithms to solve the package dependency knapsack problem.
 *
 * This class acts as a solver engine for a variation of the classic knapsack problem,
 * where items (Packages) have inter-dependencies. It orchestrates constructive heuristics
 * and delegates to metaheuristic classes like VND and VNS for solution improvement. 🧠⚙️
 */
class Algorithm {
public:
    enum class ALGORITHM_TYPE {
        NONE,
        RANDOM,
        GREEDY_PACKAGE_BENEFIT,
        GREEDY_PACKAGE_BENEFIT_RATIO,
        GREEDY_PACKAGE_SIZE,
        RANDOM_GREEDY_PACKAGE_BENEFIT,
        RANDOM_GREEDY_PACKAGE_BENEFIT_RATIO,
        RANDOM_GREEDY_PACKAGE_SIZE,
        VND,
        VNS,
        GRASP,
        GRASP_VNS
    };

    enum class LOCAL_SEARCH {
        FIRST_IMPROVEMENT,
        BEST_IMPROVEMENT,
        RANDOM_IMPROVEMENT,
        NONE,
        VNS
    };

    explicit Algorithm(double maxTime, unsigned int seed, const std::string& outputDir, const std::string& inputFile, const std::string &fileId);

    std::vector<std::unique_ptr<Bag>> run(const ProblemInstance& problemInstance, const std::string& timestamp);

    std::string toString(Algorithm::ALGORITHM_TYPE algorithm) const;
    std::string toString(Algorithm::LOCAL_SEARCH localSearch) const;

private:

    void precomputeDependencyGraph(const std::vector<Package*>& packages,
                                   const std::vector<Dependency*>& dependencies);

    const std::string m_outputDir;
    const std::string m_inputFile;
    const std::string m_fileId;
    std::string m_timestamp;
    std::unordered_map<const Package*, std::vector<const Dependency*>> m_dependencyGraph;
    const double m_maxTime;
    std::mt19937 m_generator;
};

#endif // ALGORITHM_H