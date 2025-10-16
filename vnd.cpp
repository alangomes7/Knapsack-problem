#include "vnd.h"

#include <chrono>
#include <limits>
#include <algorithm>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

VND::VND(double maxTime)
    : m_maxTime(maxTime), m_searchEngine(0), m_helper() {}

VND::VND(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_searchEngine(seed), m_helper(seed) {}

Bag* VND::run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
              const std::vector<Algorithm::LOCAL_SEARCH>& neighborhoods,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VND);

    if (neighborhoods.empty()) {
        m_helper.makeItFeasible(*bestBag, bagSize, dependencyGraph);
        return bestBag;
    }

    int k = 0;
    const int k_max = static_cast<int>(neighborhoods.size());
    bestBag->setMetaheuristicParameters("k_max=" + std::to_string(k_max));

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    // === Variable Neighborhood Descent Loop ===
    while (k < k_max) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds)
            break;

        auto currentSearch = neighborhoods[k];

        // Clone current best and apply local search
        Bag* newBag = new Bag(*bestBag);
        m_searchEngine.localSearch(*newBag, bagSize, allPackages,
                                          currentSearch, dependencyGraph, 150);
        newBag->setLocalSearch(currentSearch);

        bool improved = newBag->getBenefit() > bestBag->getBenefit();
        // If improvement, restart from first neighborhood
        if (improved) {
            delete bestBag;
            bestBag = newBag;
            k = 0; // restart exploration
        } else {
            delete newBag;
            ++k; // move to next neighborhood
        }
    }

    // === Repair final solution ===
    m_helper.makeItFeasible(*bestBag, bagSize, dependencyGraph);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());

    return bestBag;
}