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
 */
class VNS {
public:
    explicit VNS(double maxTime);
    VNS(double maxTime, unsigned int seed);

    /**
     * @brief Executes the VNS algorithm to improve a solution.
     * @param bagSize Maximum bag capacity.
     * @param initialBag The starting solution.
     * @param allPackages List of all available packages.
     * @param localSearchMethod The local search strategy to apply after shaking.
     * @param dependencyGraph A pre-computed graph for efficient dependency lookups.
     * @return A pointer to a new `Bag` object with the improved solution. Caller owns the memory.
     */
    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    Bag* shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize,
               const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                     Algorithm::LOCAL_SEARCH localSearchMethod,
                     const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                 const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                  const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const;
    
    bool removePackagesToFit(Bag& bag, int maxCapacity, const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    int randomNumberInt(int min, int max);

    const double m_maxTime; ///< Timeout in seconds.
    std::mt19937 m_generator; ///< Random number generator for stochastic parts.
};

#endif // VNS_H