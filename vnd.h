#ifndef VND_H
#define VND_H

#include <vector>
#include <unordered_map>
#include "algorithm.h"

// Forward declarations to avoid circular dependencies
class Bag;
class Package;
class Dependency;
class Algorithm;

/**
 * @brief Implements the Variable Neighborhood Descent (VND) metaheuristic.
 *
 * VND is a local search method that systematically explores a predefined neighborhood
 * structure to find improved solutions. This implementation uses a swap neighborhood
 * and a best-improvement strategy. The algorithm attempts to improve a given 
 * initial solution by iteratively searching for better solutions in its 
 * neighborhood. If a better solution is found, it replaces the current solution and 
 * the search continues from there. If no better solution is found in the current 
 * neighborhood, the algorithm terminates, having found a local optimum.
 */
class VND {
public:
    /**
     * @brief Constructs the VND solver.
     * @param maxTime A timeout in seconds to prevent excessive execution time.
     */
    explicit VND(double maxTime);

    /**
     * @brief Executes the VND algorithm to improve an initial solution.
     * @param bagSize The maximum capacity of the bag.
     * @param initialBag The starting solution to improve upon. The VND algorithm 
     * will not modify this bag, but will use it as a starting point for its search.
     * @param allPackages A list of all available packages for neighborhood generation. 
     * This list is used to find packages that are not currently in the bag and 
     * could potentially be swapped with packages that are in the bag.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups. 
     * This graph is used to quickly find the dependencies of a package, which is 
     * necessary to calculate the size of the bag after a swap.
     * @return A pointer to a new `Bag` object with the improved solution. Caller owns the memory.
     */
    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    /**
     * @brief Explores the swap neighborhood and applies the best found improvement.
     * @param currentBag The solution to be improved. This bag is modified in place.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A list of all available packages.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return `true` if an improvement was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    /**
     * @brief Evaluates if a potential swap is feasible and improves the solution.
     * @param currentBag The current solution bag.
     * @param packageIn Package to be removed from the bag.
     * @param packageOut Package to be added to the bag.
     * @param bagSize Maximum allowed bag size.
     * @param currentBenefit The current total benefit of the bag.
     * @param benefitIncrease Output parameter for the calculated benefit increase.
     * @return `true` if the swap is feasible and strictly improves benefit, `false` otherwise.
     */
    bool evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const;
    
    /**
     * @brief Greedily removes packages to ensure the bag is within capacity.
     * @param bag The bag to repair.
     * @param maxCapacity The target maximum capacity.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return `true` if the bag was brought within the size limit.
     */
    bool removePackagesToFit(Bag& bag, int maxCapacity, const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    const double m_maxTime; ///< Timeout in seconds.
};

#endif // VND_H