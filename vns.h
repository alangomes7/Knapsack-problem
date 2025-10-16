#ifndef VNS_H
#define VNS_H

#include <vector>
#include <random>
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
 * @brief Implements the Variable Neighborhood Search (VNS) metaheuristic.
 *
 * VNS is a metaheuristic that explores the solution space by systematically 
 * changing the neighborhood structure during the search. It escapes local optima 
 * by applying a "shake" perturbation to the current solution and then performing 
 * a local search from the perturbed solution.
 */
class VNS {
public:
    /**
     * @brief Constructor for the VNS class.
     * @param maxTime The maximum execution time in seconds.
     */
    explicit VNS(double maxTime);

    /**
     * @brief Constructor for the VNS class with a specific seed.
     * @param maxTime The maximum execution time in seconds.
     * @param seed The seed for the random number generator.
     */
    VNS(double maxTime, unsigned int seed);

    /**
     * @brief Runs the VNS algorithm.
     * @param bagSize The maximum capacity of the knapsack.
     * @param initialBag The initial solution to start from.
     * @param allPackages A vector of all available packages.
     * @param localSearchMethod The local search method to use.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the best found solution (Bag).
     */
    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    /**
     * @brief Shakes the current solution to escape local optima.
     * @param currentBag The current solution to be shaken.
     * @param k The neighborhood size for the shake operation.
     * @param allPackages A vector of all available packages.
     * @param bagSize The maximum capacity of the knapsack.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the new, shaken solution (Bag).
     */
    Bag* shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize,
               const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    const double m_maxTime; ///< Timeout in seconds.
    SearchEngine m_searchEngine;
    MetaheuristicHelper m_helper;
};

#endif // VNS_H