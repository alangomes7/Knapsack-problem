#include "vns_helper.h"
#include "solution_repair.h"
#include "algorithm.h"
#include "random_provider.h"
#include <algorithm>

namespace VNS_HELPER {

std::unique_ptr<Bag> shake(
    const Bag& currentBag,
    int k,
    const std::vector<Package*>& allPackages,
    int bagSize,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    std::mt19937& generator,
    std::vector<Package*>& tmpOutside)
{
    auto newBag = std::make_unique<Bag>(currentBag);
    const auto& packagesInBag = newBag->getPackages();

    // --- 1. Build list of packages NOT in the bag ---
    tmpOutside.clear();
    tmpOutside.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (packagesInBag.count(p) == 0) tmpOutside.push_back(p);
    }

    // --- 2. Remove 'k' packages (Safe and Efficient Method) ---

    // Copy packages from the set to a vector for shuffling
    std::vector<const Package*> packagesToRemove(packagesInBag.begin(), packagesInBag.end());
    
    // Shuffle the vector using the provided generator
    std::shuffle(packagesToRemove.begin(), packagesToRemove.end(), generator);

    // Determine how many to remove (at most 'k' or all of them)
    int removeCount = std::min<int>(k, static_cast<int>(packagesToRemove.size()));

    // Remove the first 'k' packages from the shuffled list
    for (int i = 0; i < removeCount; ++i) {
        const Package* pkg = packagesToRemove[i];
        newBag->removePackage(*pkg, dependencyGraph.at(pkg));
    }

    // --- 3. Add up to 'k' new packages ---

    // Shuffle the list of packages that are outside the bag
    std::shuffle(tmpOutside.begin(), tmpOutside.end(), generator);

    int added = 0;
    for (Package* pkg : tmpOutside) {
        // Stop if we have added 'k' packages
        if (added >= k) break; 
        
        const auto& deps = dependencyGraph.at(pkg);
        
        if (newBag->addPackageIfPossible(*pkg, bagSize, deps)) {
            ++added;
        }
    }

    return newBag;
}

void vnsLoop(Bag& bestBag, int bagSize,
             const std::vector<Package*>& allPackages,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
             SearchEngine& searchEngine,
             int maxLS_IterationsWithoutImprovement,
             int maxLS_Iterations,
             const std::chrono::steady_clock::time_point& deadline)
{
    const std::vector<SEARCH_ENGINE::MovementType> movements = {
        SEARCH_ENGINE::MovementType::ADD,
        SEARCH_ENGINE::MovementType::SWAP_REMOVE_1_ADD_1,
        SEARCH_ENGINE::MovementType::SWAP_REMOVE_1_ADD_2,
        SEARCH_ENGINE::MovementType::SWAP_REMOVE_2_ADD_1,
        SEARCH_ENGINE::MovementType::EJECTION_CHAIN
    };
    ALGORITHM::LOCAL_SEARCH searchMethod = ALGORITHM::LOCAL_SEARCH::BEST_IMPROVEMENT;
    const int k_max = static_cast<int>(movements.size());

    std::unique_ptr<Bag> workingBest = std::make_unique<Bag>(bestBag);
    std::vector<Package*> tmpOutside;

    int k = 0;
    while (k < k_max && std::chrono::steady_clock::now() < deadline) {
        // Sequential shake + local search
        auto shakenBag = shake(*workingBest, k + 1, allPackages, bagSize, dependencyGraph, searchEngine.getRandomGenerator(), tmpOutside);
        SOLUTION_REPAIR::repair(*shakenBag, bagSize, dependencyGraph, searchEngine.getSeed());
        searchEngine.localSearch(*shakenBag, bagSize, allPackages, movements[k],
                                 searchMethod, dependencyGraph,
                                 maxLS_IterationsWithoutImprovement, maxLS_Iterations, deadline);
        shakenBag->setMovementType(movements[k]);
        SOLUTION_REPAIR::repair(*shakenBag, bagSize, dependencyGraph, searchEngine.getSeed());

        if (shakenBag->getBenefit() > workingBest->getBenefit()) {
            workingBest = std::move(shakenBag);
            k = 0;
        } else {
            ++k;
        }
    }

    bestBag = std::move(*workingBest);
}

} // namespace VNS_HELPER