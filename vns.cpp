#include "vns.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "vns_helper.h"
#include <chrono>
#include <algorithm>

VNS::VNS(double maxTime, unsigned int seed) 
    : m_maxTime(maxTime), m_searchEngine(seed) {}

std::unique_ptr<Bag> VNS::run(
    int bagSize,
    const Bag* initialBag,
    const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (!initialBag) 
        return std::make_unique<Bag>(ALGORITHM::ALGORITHM_TYPE::NONE, "0");

    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time +
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(m_maxTime)
    );

    auto bestBag = std::make_unique<Bag>(*initialBag);

    const int k_max = 5;
    const int maxIterations = 200;
    const int maxNoImprovement = 2000;

    SearchEngine localEngine(m_searchEngine.getSeed());

    for (int iter = 0; iter < maxIterations; ++iter) {
        bool improvementFound = false;

        // Sequential evaluation of neighborhoods
        for (int k = 1; k <= k_max; ++k) {
            auto candidate = std::make_unique<Bag>(*bestBag);
            VNS_HELPER::vnsLoop(*candidate, bagSize, allPackages, dependencyGraph,
                                localEngine, 1, 1, deadline);

            if (candidate->getBenefit() > bestBag->getBenefit()) {
                *bestBag = *candidate;
                improvementFound = true;
            }
        }

        if (!improvementFound) break;
        if (std::chrono::steady_clock::now() >= deadline) break;
    }

    auto end_time = std::chrono::steady_clock::now();
    bestBag->setAlgorithmTime(std::chrono::duration<double>(end_time - start_time).count());
    bestBag->setBagAlgorithm(ALGORITHM::ALGORITHM_TYPE::VNS);
    bestBag->setMetaheuristicParameters("k_max=" + std::to_string(k_max));

    return bestBag;
}
