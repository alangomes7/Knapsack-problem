#include "vns.h"
#include <chrono>
#include <algorithm>
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "vns_helper.h"

VNS::VNS(double maxTime, unsigned int seed) : m_maxTime(maxTime), m_searchEngine(seed) {}

std::unique_ptr<Bag> VNS::run(int bagSize, const Bag* initialBag,
                              const std::vector<Package*>& allPackages,
                              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (!initialBag) {
        return std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::NONE, "0");
    }

    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(m_maxTime));

    // The bestBag is now the one provided, copied
    auto bestBag = std::make_unique<Bag>(*initialBag);
    
    // Call the centralized VNS loop logic
    // We pass our single m_searchEngine, as VNS is single-threaded
    VnsHelper::vnsLoop(
        *bestBag,
        bagSize,
        allPackages,
        dependencyGraph,
        m_searchEngine,
        200,   // MAX_ITERATIONS
        2000,  // MAX_NO_IMPROVEMENT
        deadline
    );

    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VNS);
    bestBag->setLocalSearch(Algorithm::LOCAL_SEARCH::NONE); // VNS is the metaheuristic
    bestBag->setMetaheuristicParameters("k_max=5"); // k_max is hardcoded in helper

    return bestBag;
}