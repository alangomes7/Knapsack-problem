#include "vnd.h"

#include <chrono>
#include <limits>
#include <algorithm>

#include "bag.h"
#include "package.h"
#include "dependency.h"

VND::VND(double maxTime) : m_maxTime(maxTime) {}

Bag* VND::run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VND);
    bestBag->setLocalSearch(Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT);
    bool improved = true;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (improved) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }

        improved = m_localSearch.run(*bestBag, bagSize * 1.5, allPackages, Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT, dependencyGraph, 200);
        m_helper.makeItFeasible(*bestBag, bagSize, dependencyGraph);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());
    return bestBag;
}