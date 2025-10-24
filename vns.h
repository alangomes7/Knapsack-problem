#ifndef VNS_H
#define VNS_H

#include <vector>
#include <random>
#include <unordered_map>
#include <memory>
#include "algorithm.h"
#include "search_engine.h"
#include "solution_repair.h"

// Forward declarations
class Bag;
class Package;
class Dependency;

/**
 * @brief Implements the Variable Neighborhood Search (VNS) metaheuristic.
 *
 * VNS explores the solution space by systematically changing the neighborhood structure.
 * It escapes local optima by "shaking" the current solution and then performing a local search.
 */
class VNS {
public:
    VNS(double maxTime, unsigned int seed);

    /**
     * @brief Runs the VNS algorithm.
     * @param bagSize Maximum capacity of the knapsack.
     * @param initialBag Optional initial Bag to improve.
     * @param allPackages Vector of all available packages.
     * @param dependencyGraph Precomputed graph of package dependencies.
     * @return Unique pointer to the best found solution.
     */
    std::unique_ptr<Bag> run(
        int bagSize,
        const Bag* initialBag,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
    );

private:
    /**
     * @brief Shakes the current solution to escape local optima.
     * @param currentBag Current solution to shake.
     * @param k Neighborhood size for the shake operation.
     * @param allPackages Vector of all available packages.
     * @param bagSize Maximum bag capacity.
     * @param dependencyGraph Precomputed package dependency graph.
     * @return Unique pointer to the new, shaken solution.
     */
    std::unique_ptr<Bag> shake(
        const Bag& currentBag,
        int k,
        const std::vector<Package*>& allPackages,
        int bagSize,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
    );

private:
    const double m_maxTime; ///< Maximum allowed execution time in seconds.
    SearchEngine m_searchEngine;
};

#endif // VNS_H
