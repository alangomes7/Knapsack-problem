#ifndef GRASP_H
#define GRASP_H

#include <vector>
#include <random>
#include <unordered_map>
#include "algorithm.h"

// Forward declarations
class Bag;
class Package;
class Dependency;
class Algorithm;

class GRASP {
public:
    explicit GRASP(double maxTime);
    GRASP(double maxTime, unsigned int seed);

    Bag* run(int bagSize, const std::vector<Bag*>& initialBags, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    Bag* construction(int bagSize, const std::vector<Package*>& allPackages,
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                     Algorithm::LOCAL_SEARCH localSearchMethod,
                     const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const;
    
    int randomNumberInt(int min, int max);

    const double m_maxTime; ///< Timeout in seconds.
    std::mt19937 m_generator; ///< Random number generator for stochastic parts.
};

#endif // GRASP_H