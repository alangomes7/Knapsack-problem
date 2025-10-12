#ifndef LOCALSEARCH_H
#define LOCALSEARCH_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "algorithm.h"
#include "MetaheuristicHelper.h"

class Bag;
class Package;
class Dependency;

/**
 * @brief Provides optimized local search algorithms for the knapsack problem.
 *
 * This class implements various local search strategies with performance optimizations:
 * - Sorted neighborhoods for better pruning
 * - Early termination conditions
 * - Parallel processing for best-improvement
 * - Adaptive sampling for random-improvement
 * - Iteration limits to prevent infinite loops
 */
class LocalSearch {
public:
    bool run(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph, int maxInterations);

private:
    bool exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                 const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                  const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    void buildOutsidePackages(const std::unordered_set<const Package*>& packagesInBag, 
                             const std::vector<Package*>& allPackages, 
                             std::vector<Package*>& packagesOutsideBag);
    
    MetaheuristicHelper m_metaheuristicHelper;
};

#endif // LOCALSEARCH_H