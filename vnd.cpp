#include "vnd.h"

#include <chrono>
#include <limits>
#include <algorithm>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

VND::VND(double maxTime)
    : m_maxTime(maxTime), m_searchEngine(0), m_metaheuristicHelper() {}

VND::VND(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_searchEngine(seed), m_metaheuristicHelper(seed) {}

Bag* VND::run(int bagSize, const Bag* initialBag,
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

    int k = 0;
    const int k_max = static_cast<int>(movements.size());
    
    // Work on a local copy of the initial bag
    std::unique_ptr<Bag> bestBag = std::make_unique<Bag>(*initialBag);
    bestBag->setMetaheuristicParameters("k_max=" + std::to_string(k_max));

    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(m_maxTime));

    // === Variable Neighborhood Descent Loop ===
    while (k < k_max) {
        if (std::chrono::steady_clock::now() > deadline){
            break;
        }

        // Create a local copy for current neighborhood search
        std::unique_ptr<Bag> currentBagSearch = std::make_unique<Bag>(*bestBag);

        // Perform local search for this neighborhood
        m_searchEngine.localSearch(
            *currentBagSearch,
            bagSize,
            allPackages,
            movements[k],
            searchMethod,
            dependencyGraph,
            200,
            2000,
            deadline
        );
        currentBagSearch->setMovementType(movements[k]);

        m_metaheuristicHelper.makeItFeasible(*currentBagSearch, bagSize, dependencyGraph);

        bool improved = currentBagSearch->getBenefit() > bestBag->getBenefit();

        if (improved) {
            bestBag = std::move(currentBagSearch);
            k = 0; // Restart from first neighborhood
        } else {
            ++k;
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;

    bestBag->setAlgorithmTime(elapsed_seconds.count());
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VND);
    bestBag->setLocalSearch(Algorithm::LOCAL_SEARCH::NONE);

    return new Bag(*bestBag);
}