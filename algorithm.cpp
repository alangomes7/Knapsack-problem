#include "algorithm.h"

#include <chrono>
#include <utility>
#include <algorithm>
#include <iterator>
#include <limits>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "SearchStrategies.h"
#include "searchEngine.h"
#include "MetaheuristicHelper.h"
#include "vnd.h"
#include "vns.h"

Algorithm::Algorithm(double maxTime)
    : m_maxTime(maxTime), m_metaheuristicHelper()
{
}

Algorithm::Algorithm(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_generator(seed), m_metaheuristicHelper(seed)
{
}

// =============================================================
// == Main Control: Executes all strategies (construct + improve)
// =============================================================
std::vector<Bag*> Algorithm::run(Algorithm::ALGORITHM_TYPE algorithm,
                                 int bagSize,
                                 const std::vector<Package*>& packages,
                                 const std::vector<Dependency*>& dependencies,
                                 const std::string& timestamp)
{
    m_timestamp = timestamp;

    // Precompute dependencies once
    precomputeDependencyGraph(packages, dependencies);

    // Create the strategy manager (constructive phase)
    SearchStrategies strategies(
        m_maxTime,
        m_generator,              // share RNG
        m_metaheuristicHelper,    // share helper
        m_dependencyGraph,        // dependency map
        m_timestamp
    );

    std::vector<Bag*> resultBag;
    resultBag.reserve(10);

    // === Constructive Phase ===
    resultBag.push_back(strategies.randomBag(bagSize, packages));
    std::vector<Bag*> greedyBags = strategies.greedyBag(bagSize, packages);
    std::vector<Bag*> randomGreedyBags = strategies.randomGreedy(bagSize, packages);

    resultBag.insert(resultBag.end(), greedyBags.begin(), greedyBags.end());
    resultBag.insert(resultBag.end(), randomGreedyBags.begin(), randomGreedyBags.end());

    // === Select best initial bag ===
    Bag* bestInitialBag = nullptr;
    int bestInitialBenefit = -1;
    for (Bag* bag : resultBag) {
        if (bag->getBenefit() > bestInitialBenefit) {
            bestInitialBenefit = bag->getBenefit();
            bestInitialBag = bag;
        }
    }

    // === Improvement Phase (Metaheuristics) ===
    if (bestInitialBag) {
        // VND
        VND vnd(m_maxTime, m_generator());
        Bag* vndBag = vnd.run(bagSize, bestInitialBag, packages, m_dependencyGraph);
        vndBag->setTimestamp(m_timestamp);
        

        // VNS
        VNS vns(m_maxTime, m_generator());
        Bag* vnsBag = vns.run(bagSize, bestInitialBag, packages, m_dependencyGraph);
;

        // GRASP (combines best previous results)
        // GRASP grasp(m_maxTime, m_generator());
        // Bag* graspBag = grasp.run(bagSize, resultBag, packages, SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
        //                            Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT, m_dependencyGraph);
        // graspBag->setTimestamp(m_timestamp);
        
        resultBag.push_back(vndBag);
        resultBag.push_back(vnsBag);
        // resultBag.push_back(graspBag);
    }

    return resultBag;
}

// =============================================================
// == String Conversions
// =============================================================

std::string Algorithm::toString(Algorithm::ALGORITHM_TYPE algorithm) const {
    switch (algorithm) {
        case Algorithm::ALGORITHM_TYPE::RANDOM: return "RANDOM";
        case Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit: return "GREEDY -> Package: Benefit";
        case Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio: return "GREEDY -> Package: Benefit Ratio";
        case Algorithm::ALGORITHM_TYPE::GREEDY_Package_Size: return "GREEDY -> Package: Size";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit: return "RANDOM_GREEDY (Package: Benefit)";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio: return "RANDOM_GREEDY (Package: Benefit Ratio)";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size: return "RANDOM_GREEDY (Package: Size)";
        case Algorithm::ALGORITHM_TYPE::VND: return "VND";
        case Algorithm::ALGORITHM_TYPE::VNS: return "VNS";
        case Algorithm::ALGORITHM_TYPE::GRASP: return "GRASP";
        default: return "NONE";
    }
}

std::string Algorithm::toString(Algorithm::LOCAL_SEARCH localSearch) const {
    switch (localSearch) {
        case Algorithm::LOCAL_SEARCH::FIRST_IMPROVEMENT: return "First Improvement";
        case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT: return "Best Improvement";
        case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT: return "Random Improvement";
        default: return "None";
    }
}

// =============================================================
// == Dependency Precomputation (kept here)
// =============================================================
void Algorithm::precomputeDependencyGraph(const std::vector<Package*>& packages,
                                          const std::vector<Dependency*>& dependencies)
{
    m_dependencyGraph.clear();
    m_dependencyGraph.reserve(packages.size());

    for (const auto* package : packages) {
        if (package) {
            const auto& packageDependencies = package->getDependencies();
            std::vector<const Dependency*> depVector;
            depVector.reserve(packageDependencies.size());
            for (const auto& pair : packageDependencies) {
                depVector.push_back(pair.second);
            }
            m_dependencyGraph[package] = std::move(depVector);
        }
    }
}
