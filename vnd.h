#ifndef VND_H
#define VND_H

#include <vector>
#include <unordered_map>
#include <memory>

#include "algorithm.h"
#include "search_engine.h"

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
    explicit VND(double maxTime, unsigned int seed);

    /**
     * @brief Runs the VND algorithm.
     * @param bagSize The maximum capacity of the knapsack.
     * @param initialBag Intial Bag to search fro improvements.
     * @param allPackages A vector of all available packages.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the best found solution (Bag).
     */
    std::unique_ptr<Bag> run(int bagSize, const Bag* initialBag,
              const std::vector<Package*>& allPackages,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    const double m_maxTime;
    SearchEngine m_searchEngine;
};

#endif // VND_H