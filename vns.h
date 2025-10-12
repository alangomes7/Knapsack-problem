#ifndef VNS_H
#define VNS_H

#include <vector>
#include <random>
#include <unordered_map>
#include "algorithm.h"

// Forward declarations
class Bag;
class Package;
class Dependency;
class Algorithm;

/**
 * @brief Implements the Variable Neighborhood Search (VNS) metaheuristic.
 *
 * VNS is designed to escape local optima by systematically changing neighborhood
 * structures during the search via a "shaking" process, followed by local search.
 * The shaking process perturbs the current solution to a new one in a different
 * neighborhood, from which a local search is then performed. If the new solution
 * is better than the current one, it is accepted and the search continues from
 * there. Otherwise, the search continues from the current solution, but with a
 * different neighborhood.
 */
class VNS {
public:
    /**
     * @brief Constructs the VNS solver with a given maximum execution time.
     * @param maxTime The maximum time in seconds that the VNS algorithm can run.
     */
    explicit VNS(double maxTime);

    /**
     * @brief Constructs the VNS solver with a given maximum execution time and a seed for the random number generator.
     * @param maxTime The maximum time in seconds that the VNS algorithm can run.
     * @param seed The seed for the random number generator.
     */
    VNS(double maxTime, unsigned int seed);

    /**
     * @brief Executes the VNS algorithm to improve a solution.
     * @param bagSize Maximum bag capacity.
     * @param initialBag The starting solution. The VNS algorithm will not modify this bag, but will use it as a starting point for its search.
     * @param allPackages List of all available packages.
     * @param localSearchMethod The local search strategy to apply after shaking. This can be one of the following: FIRST_IMPROVEMENT, BEST_IMPROVEMENT, or RANDOM_IMPROVEMENT.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return A pointer to a new `Bag` object with the improved solution. Caller owns the memory.
     */
    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    /**
     * @brief Perturbs the current solution to a new one in a different neighborhood.
     * @param currentBag The current solution.
     * @param k The size of the neighborhood to shake to.
     * @param allPackages List of all available packages.
     * @param bagSize Maximum bag capacity.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return A pointer to a new `Bag` object with the perturbed solution. Caller owns the memory.
     */
    Bag* shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize,
               const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    /**
     * @brief Performs a local search from a given solution.
     * @param currentBag The solution to be improved. This bag is modified in place.
     * @param bagSize Maximum bag capacity.
     * @param allPackages List of all available packages.
     * @param localSearchMethod The local search strategy to apply.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return `true` if an improvement was found and applied, `false` otherwise.
     */
    bool localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                     Algorithm::LOCAL_SEARCH localSearchMethod,
                     const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    /**
     * @brief Explores the swap neighborhood and applies the first found improvement.
     * @param currentBag The solution to be improved. This bag is modified in place.
     * @param bagSize Maximum bag capacity.
     * @param allPackages List of all available packages.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return `true` if an improvement was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                 const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    /**
     * @brief Explores the swap neighborhood and applies the best found improvement.
     * @param currentBag The solution to be improved. This bag is modified in place.
     * @param bagSize Maximum bag capacity.
     * @param allPackages List of all available packages.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return `true` if an improvement was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    /**
     * @brief Explores the swap neighborhood and applies a randomly chosen improvement.
     * @param currentBag The solution to be improved. This bag is modified in place.
     * @param bagSize Maximum bag capacity.
     * @param allPackages List of all available packages.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return `true` if an improvement was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
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
    
    /**
     * @brief Generates a random integer in a given range.
     * @param min The minimum value of the range.
     * @param max The maximum value of the range.
     * @return A random integer in the range [min, max].
     */
    int randomNumberInt(int min, int max);

    const double m_maxTime; ///< Timeout in seconds.
    std::mt19937 m_generator; ///< Random number generator for stochastic parts.
};

#endif // VNS_H