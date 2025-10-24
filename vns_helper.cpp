#include "vns_helper.h"
#include "random_provider.h"
#include "solution_repair.h"
#include "algorithm.h" // For Algorithm enums
#include <algorithm>   // For std::shuffle

namespace VnsHelper {

std::unique_ptr<Bag> shake(const Bag& currentBag, int k,
                            const std::vector<Package*>& allPackages, int bagSize,
                            const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    auto newBag = std::make_unique<Bag>(currentBag);
    const auto& packagesInBag = newBag->getPackages();

    // 1. Remove k random packages
    for (int i = 0; i < k && !packagesInBag.empty(); ++i) {
        int offset = RandomProvider::getInt(0, packagesInBag.size() - 1);
        auto it = packagesInBag.begin();
        std::advance(it, offset);
        const Package* packageToRemove = *it;
        const auto& deps = dependencyGraph.at(packageToRemove);
        newBag->removePackage(*packageToRemove, deps);
    }

    // 2. Add k random packages
    std::vector<Package*> packagesOutside;
    packagesOutside.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (newBag->getPackages().count(p) == 0) {
            packagesOutside.push_back(p);
        }
    }
    
    // Shuffle to randomize the order of additions
    std::shuffle(packagesOutside.begin(), packagesOutside.end(), RandomProvider::getGenerator());

    int addedCount = 0;
    for (Package* packageToAdd : packagesOutside) {
        if (addedCount >= k) break; // Stop after adding k packages
        
        const auto& deps = dependencyGraph.at(packageToAdd);
        if (newBag->canAddPackage(*packageToAdd, bagSize, deps)) {
            newBag->addPackage(*packageToAdd, deps);
            ++addedCount;
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
    // Define the neighborhood structures (movements)
    const std::vector<SearchEngine::MovementType> movements = {
        SearchEngine::MovementType::ADD,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2,
        SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1,
        SearchEngine::MovementType::EJECTION_CHAIN
    };
    
    Algorithm::LOCAL_SEARCH searchMethod = Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT;
    const int k_max = static_cast<int>(movements.size());
    int k = 0;

    // Create a working copy. 'bestBag' (the input) is our best-so-far.
    std::unique_ptr<Bag> workingBest = std::make_unique<Bag>(bestBag);

    while (k < k_max) {
        if (std::chrono::steady_clock::now() > deadline) {
            break;
        }

        // 1. Shake: Perturb the *current best* solution
        auto shakenBag = VnsHelper::shake(
            *workingBest, k + 1, allPackages, bagSize, dependencyGraph
        );
        SolutionRepair::repair(*shakenBag, bagSize, dependencyGraph);

        // 2. Local Search: Apply LS using the k-th neighborhood
        searchEngine.localSearch(
            *shakenBag,
            bagSize,
            allPackages,
            movements[k], // Use the k-th movement type
            searchMethod,
            dependencyGraph,
            maxLS_IterationsWithoutImprovement,
            maxLS_Iterations,
            deadline
        );

        shakenBag->setMovementType(movements[k]);
        SolutionRepair::repair(*shakenBag, bagSize, dependencyGraph);

        // 3. Neighborhood Change: Move to new solution if it's better
        if (shakenBag->getBenefit() > workingBest->getBenefit()) {
            workingBest = std::move(shakenBag);
            k = 0; // Reset to the first neighborhood
        } else {
            ++k; // Try the next neighborhood
        }
    }

    // Update the original bag with the VNS-improved solution
    bestBag = std::move(*workingBest);
}

} // namespace VnsHelper