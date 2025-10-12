#ifndef VND_H
#define VND_H

#include <vector>
#include <unordered_map>
#include "algorithm.h"
#include "localSearch.h"
#include "metaheuristicHelper.h"

// Forward declarations to avoid circular dependencies
class Bag;
class Package;
class Dependency;
class Algorithm;

/**
 * @brief Implements the Variable Neighborhood Descent (VND) metaheuristic.
 *
 * VND is a metaheuristic that systematically explores different neighborhood 
 * structures to find a local optimum. This implementation uses a single 
 * neighborhood (swap) but iteratively applies a local search and a repair 
 * mechanism to improve the solution.
 */
class VND {
public:
    /**
     * @brief Constructor for the VND class.
     * @param maxTime The maximum execution time in seconds.
     */
    explicit VND(double maxTime);

    /**
     * @brief Runs the VND algorithm.
     * @param bagSize The maximum capacity of the knapsack.
     * @param initialBag The initial solution to start from.
     * @param allPackages A vector of all available packages.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the best found solution (Bag).
     */
    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    const double m_maxTime; ///< Timeout in seconds.
    LocalSearch m_localSearch;
    MetaheuristicHelper m_helper;
};

#endif // VND_H