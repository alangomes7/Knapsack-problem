#ifndef LOCALSEARCH_H
#define LOCALSEARCH_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "algorithm.h"

// Forward declarations to avoid including full headers
class Bag;
class Package;
class Dependency;

/**
 * @brief Provides optimized local search algorithms for the knapsack problem,
 * ensuring all moves maintain solution feasibility.
 */
class LocalSearch {
public:
    /**
     * @brief Runs the local search metaheuristic.
     * * @param currentBag The current solution to be improved.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A list of all available packages.
     * @param localSearchMethod The neighborhood exploration strategy (First, Best, Random).
     * @param dependencyGraph The graph defining package dependencies.
     * @param maxIterations The maximum number of iterations without improvement before stopping.
     * @return True if the solution was improved, false otherwise.
     */
    bool run(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph, 
             int maxIterations);

private:
    // --- Move Operators: Each operator ensures the move is feasible before applying it ---

    bool tryAddPackage(Bag& currentBag, int bagSize, 
                      const std::vector<Package*>& packagesOutsideBag, 
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool tryRemovePackage(Bag& currentBag, 
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    // --- Neighborhood Exploration Strategies ---

    bool exploreSwapNeighborhoodFirstImprovement(
        Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodRandomImprovement(
        Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodBestImprovement(
        Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    // --- Utility Function ---

    void buildOutsidePackages(const std::unordered_set<const Package*>& packagesInBag, 
                             const std::vector<Package*>& allPackages, 
                             std::vector<Package*>& packagesOutsideBag);
};

#endif // LOCALSEARCH_H