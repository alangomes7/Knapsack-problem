#include "vns.h"

#include <chrono>
#include <limits>
#include <algorithm>
#include <iterator>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

VNS::VNS(double maxTime) : m_maxTime(maxTime), m_searchEngine(0), m_metaheuristicHelper() {}

VNS::VNS(double maxTime, unsigned int seed) : m_maxTime(maxTime), m_searchEngine(seed), m_metaheuristicHelper(seed) {}

Bag* VNS::run(int bagSize, const Bag* initialBag,
              const std::vector<Package*>& allPackages,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (!initialBag) {
        return new Bag(Algorithm::ALGORITHM_TYPE::NONE, "0");
    }

    // Neighborhood structures
    std::vector<SearchEngine::MovementType> movements = {
        SearchEngine::MovementType::ADD,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2,
        SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1,
        SearchEngine::MovementType::EJECTION_CHAIN
    };

    Algorithm::LOCAL_SEARCH searchMethod = Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT;

    const int k_max = static_cast<int>(movements.size());
    int k = 0;

    // Clone initial bag as working best solution
    std::unique_ptr<Bag> bestBag = std::make_unique<Bag>(*initialBag);
    bestBag->setMetaheuristicParameters("k_max=" + std::to_string(k_max));

    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time + std::chrono::duration<double>(m_maxTime);

    // === Variable Neighborhood Search Main Loop ===
    while (k < k_max) {
        if (std::chrono::steady_clock::now() > deadline){
            break;
        }

        // --- Shake phase ---
        std::unique_ptr<Bag> shakenBag(shake(*bestBag, k + 1, allPackages, bagSize, dependencyGraph));
        m_metaheuristicHelper.makeItFeasible(*shakenBag, bagSize, dependencyGraph);

        // --- Local Search phase ---
        m_searchEngine.localSearch(
            *shakenBag,
            bagSize,
            allPackages,
            movements[k],
            searchMethod,
            dependencyGraph,
            200,
            2000,
            deadline
        );
        shakenBag->setMovementType(movements[k]);
        m_metaheuristicHelper.makeItFeasible(*shakenBag, bagSize, dependencyGraph);

        // --- Acceptance criterion ---
        bool improved = shakenBag->getBenefit() > bestBag->getBenefit();

        if (improved) {
            bestBag = std::move(shakenBag);
            k = 0; // restart from first neighborhood
        } else {
            ++k;   // move to next neighborhood
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;

    bestBag->setAlgorithmTime(elapsed_seconds.count());
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VNS);
    bestBag->setLocalSearch(Algorithm::LOCAL_SEARCH::NONE);

    // Return a heap-allocated copy for caller ownership
    return new Bag(*bestBag);
}

Bag* VNS::shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize,
                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    Bag* newBag = new Bag(currentBag);
    const auto& packagesInBag = newBag->getPackages();

    // Remove packages
    for (int i = 0; i < k && !packagesInBag.empty(); ++i) {
        int offset = m_metaheuristicHelper.randomNumberInt(0, packagesInBag.size() - 1);
        auto it = packagesInBag.begin();
        std::advance(it, offset);
        const Package* packageToRemove = *it;
        const auto& deps = dependencyGraph.at(packageToRemove);
        newBag->removePackage(*packageToRemove, deps);
    }

    // Gets packages outside bag
    std::vector<Package*> packagesOutside;
    packagesOutside.reserve(allPackages.size());
    const auto& currentPackagesInBag = newBag->getPackages();
    for (Package* p : allPackages) {
        if (currentPackagesInBag.count(p) == 0) {
            packagesOutside.push_back(p);
        }
    }

    // Swap packages in and out bag
    for (int i = 0; i < k && !packagesOutside.empty(); ++i) {
        int index = m_metaheuristicHelper.randomNumberInt(0, packagesOutside.size() - 1);
        Package* packageToAdd = packagesOutside[index];
        const auto& deps = dependencyGraph.at(packageToAdd);
        if (newBag->canAddPackage(*packageToAdd, bagSize, deps)) {
            newBag->addPackage(*packageToAdd, deps);
        }
        packagesOutside.erase(packagesOutside.begin() + index);
    }
    return newBag;
}