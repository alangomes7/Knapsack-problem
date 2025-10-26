#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>
#include <memory>

#include "data_model.h"

class Bag;
class Package;
class Dependency;
class LocalSearch;

namespace ALGORITHM {
    
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

std::string toString(ALGORITHM_TYPE algorithm);
std::string toString(LOCAL_SEARCH localSearch);

} // namespace ALGORITHM

/**
 * @brief Provides a collection of algorithms to solve the package dependency knapsack problem.
 *
 * This class acts as a solver engine for a variation of the classic knapsack problem,
 * where items (Packages) have inter-dependencies. It orchestrates constructive heuristics
 * and delegates to metaheuristic classes like VND and VNS for solution improvement. 🧠⚙️
 */
class Algorithm {
public:

    explicit Algorithm(double maxTime, unsigned int seed);

    std::vector<std::unique_ptr<Bag>> run(const ProblemInstance& problemInstance, const std::string& timestamp);

private:

    void precomputeDependencyGraph(const std::vector<Package*>& packages,
                                   const std::vector<Dependency*>& dependencies);

    const double m_maxTime;
    unsigned int m_seed;
    std::mt19937 m_generator;
    std::string m_timestamp;
    std::unordered_map<const Package*, std::vector<const Dependency*>> m_dependencyGraph;
};

#endif // ALGORITHM_H