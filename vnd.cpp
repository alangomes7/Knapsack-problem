#include "vnd.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "solution_repair.h"
#include <chrono>
#include <algorithm>

VND::VND(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_searchEngine(seed) {}

std::unique_ptr<Bag> VND::run(int bagSize, const Bag* initialBag,
                              const std::vector<Package*>& allPackages,
                              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (!initialBag) {
        return std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::NONE, "0");
    }

    std::vector<SearchEngine::MovementType> movements = {
        SearchEngine::MovementType::ADD,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2,
        SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1,
        SearchEngine::MovementType::EJECTION_CHAIN
    };

    const int k_max = static_cast<int>(movements.size());
    Algorithm::LOCAL_SEARCH searchMethod = Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT;

    auto bestBag = std::make_unique<Bag>(*initialBag);
    bestBag->setMetaheuristicParameters("k_max=" + std::to_string(k_max));

    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(m_maxTime)
    );

    int k = 0;
    while (k < k_max) {
        if (std::chrono::steady_clock::now() > deadline) break;

        // --- Sequential neighborhood evaluation ---
        auto candidateBag = std::make_unique<Bag>(*bestBag);
        m_searchEngine.localSearch(
            *candidateBag,
            bagSize,
            allPackages,
            movements[k],
            searchMethod,
            dependencyGraph,
            200,
            2000,
            deadline
        );

        candidateBag->setMovementType(movements[k]);
        SolutionRepair::repair(*candidateBag, bagSize, dependencyGraph);

        if (candidateBag->getBenefit() > bestBag->getBenefit()) {
            bestBag = std::move(candidateBag);
            k = 0; // restart from first neighborhood
        } else {
            ++k; // move to next neighborhood
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    bestBag->setAlgorithmTime(std::chrono::duration<double>(end_time - start_time).count());
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VND);
    bestBag->setLocalSearch(Algorithm::LOCAL_SEARCH::NONE);

    return bestBag;
}