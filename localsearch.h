#ifndef LOCALSEARCH_H
#define LOCALSEARCH_H

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "algorithm.h"

class Bag;
class Package;
class Dependency;

class LocalSearch {
public:
    bool run(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    bool exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                 const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    bool exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                                                  const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const;
    
    void buildOutsidePackages(const std::unordered_set<const Package *> &packagesInBag, const std::vector<Package *> &allPackages, std::vector<Package *> &packagesOutsideBag);
};

#endif // LOCALSEARCH_H