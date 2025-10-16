#ifndef VND_H
#define VND_H

#include <vector>
#include <unordered_map>
#include "algorithm.h"
#include "searchEngine.h"
#include "metaheuristicHelper.h"

// Forward declarations
class Bag;
class Package;
class Dependency;
class Algorithm;

/**
 * @brief Enhanced Variable Neighborhood Descent (VND).
 *
 * This version explores multiple local search strategies
 * in a systematic descent fashion. If an improvement is found,
 * it restarts the search from the first neighborhood structure.
 */
class VND {
public:
    explicit VND(double maxTime);
    VND(double maxTime, unsigned int seed);

    /**
     * @brief Runs the VND algorithm.
     * @param bagSize The maximum capacity of the knapsack.
     *param initialBag The initial solution to start from.
     * @param allPackages A vector of all available packages.
     * @param neighborhoods Local search method to use as neighborhood structures.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the best found solution (Bag).
     */
    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             const std::vector<Algorithm::LOCAL_SEARCH>& neighborhoods,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    const double m_maxTime;
    SearchEngine m_searchEngine;
    MetaheuristicHelper m_helper;
};

#endif // VND_H