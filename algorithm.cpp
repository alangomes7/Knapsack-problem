#include "algorithm.h"

#include <chrono>
#include <utility>
#include <algorithm>
#include <iterator>
#include <limits>
#include <filesystem>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "constructive_solutions.h"
#include "Search_engine.h"
#include "solution_repair.h"
#include "vnd.h"
#include "vns.h"
#include "grasp.h"
#include "grasp_vns.h"
#include "file_processor.h"

namespace ALGORITHM {

// =============================================================
// == String Conversions
// =============================================================
std::string toString(ALGORITHM_TYPE algorithm)
{
    switch (algorithm)
    {
        case ALGORITHM_TYPE::RANDOM: return "RANDOM";
        case ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT: return "GREEDY_PACKAGE-BENEFIT";
        case ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT_RATIO: return "GREEDY_PACKAGE-BENEFIT_RATIO";
        case ALGORITHM_TYPE::GREEDY_PACKAGE_SIZE: return "GREEDY_PACKAGE-SIZE";
        case ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_BENEFIT: return "RANDOM_GREEDY_PACKAGE-BENEFIT";
        case ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_BENEFIT_RATIO: return "RANDOM_GREEDY_PACKAGE-BENEFIT_RATIO";
        case ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_SIZE: return "RANDOM_GREEDY_PACKAGE-SIZE";
        case ALGORITHM_TYPE::VND: return "VND";
        case ALGORITHM_TYPE::VNS: return "VNS";
        case ALGORITHM_TYPE::GRASP: return "GRASP";
        case ALGORITHM_TYPE::GRASP_VNS: return "GRASP_VNS";
        default: return "NONE";
    }
}

std::string toString(LOCAL_SEARCH localSearch)
{
    switch (localSearch)
    {
        case LOCAL_SEARCH::FIRST_IMPROVEMENT: return "First Improvement";
        case LOCAL_SEARCH::BEST_IMPROVEMENT: return "Best Improvement";
        case LOCAL_SEARCH::RANDOM_IMPROVEMENT: return "Random Improvement";
        case LOCAL_SEARCH::NONE: return "NONE";
        default: return "VNS";
    }
}

} // namespace ALGORITHM

// =============================================================
// == Constructor
// =============================================================
Algorithm::Algorithm(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_generator(seed), m_seed(seed)
{
}

// =============================================================
// == Main Control: Executes all strategies (construct + improve)
// =============================================================
std::vector<std::unique_ptr<Bag>> Algorithm::run(const ProblemInstance& problemInstance, const std::string& timestamp)
{
    m_timestamp = timestamp;

    precomputeDependencyGraph(problemInstance.packages, problemInstance.dependencies);
    ConstructiveSolutions constructiveSolutions(m_maxTime, m_generator, m_dependencyGraph, m_timestamp);

    std::vector<std::unique_ptr<Bag>> resultBag;
    resultBag.reserve(19);

    std::shared_ptr<Bag> bestInitialBag;
    int bestBenefit = std::numeric_limits<int>::min();

    auto updateBestBag = [&](const std::unique_ptr<Bag>& bag) {
        if (!bag) return;
        if (bag->getBenefit() > bestBenefit) {
            bestBenefit = bag->getBenefit();
            bestInitialBag = std::make_shared<Bag>(*bag);
        }
    };

    // === Constructive Phase ===
    resultBag.push_back(constructiveSolutions.randomBag(problemInstance.maxCapacity, problemInstance.packages));

    for (auto& bag : constructiveSolutions.greedyBag(problemInstance.maxCapacity, problemInstance.packages))
        resultBag.push_back(std::move(bag));

    for (auto& bag : constructiveSolutions.randomGreedy(problemInstance.maxCapacity, problemInstance.packages))
        resultBag.push_back(std::move(bag));

    for (auto& bag : resultBag){
        updateBestBag(bag);
        bag->setSeed(m_seed);
    }

    if (!bestInitialBag && !resultBag.empty())
        bestInitialBag = std::make_shared<Bag>(*resultBag.front());

    std::vector<SEARCH_ENGINE::MovementType> moves = {
        SEARCH_ENGINE::MovementType::ADD,
        SEARCH_ENGINE::MovementType::SWAP_REMOVE_1_ADD_1,
        SEARCH_ENGINE::MovementType::SWAP_REMOVE_1_ADD_2,
        SEARCH_ENGINE::MovementType::SWAP_REMOVE_2_ADD_1,
        SEARCH_ENGINE::MovementType::EJECTION_CHAIN
    };

    // === Improvement Phase (Sequential VND + VNS) ===
    {
        VND vnd(m_maxTime, m_generator());
        auto bagVND = vnd.run(problemInstance.maxCapacity, bestInitialBag.get(), problemInstance.packages, m_dependencyGraph);
        bagVND->setTimestamp(m_timestamp);
        updateBestBag(bagVND);
        resultBag.push_back(std::move(bagVND));

        VNS vns(m_maxTime, m_generator());
        auto bagVNS = vns.run(problemInstance.maxCapacity, bestInitialBag.get(), problemInstance.packages, m_dependencyGraph);
        bagVNS->setTimestamp(m_timestamp);
        updateBestBag(bagVNS);
        resultBag.push_back(std::move(bagVNS));
    }

    // === GRASP & GRASP_VNS Sequential (Single Loop) ===
    int maxGraspIterations = 100;
    using GraspType = std::unique_ptr<Bag>(*)(double, std::mt19937&, int, int);
    for (auto move : moves) {
        // GRASP
        {
            GRASP grasp(m_maxTime, m_generator(), static_cast<int>(problemInstance.packages.size() / 3), -1);
            auto bagGrasp = grasp.run(problemInstance.maxCapacity, problemInstance.packages, move, m_dependencyGraph, 200, maxGraspIterations);
            bagGrasp->setTimestamp(m_timestamp);
            updateBestBag(bagGrasp);
            resultBag.push_back(std::move(bagGrasp));
        }

        // GRASP_VNS
        {
            GRASP_VNS graspVNS(m_maxTime, m_generator(), static_cast<int>(problemInstance.packages.size() / 3), -1);
            auto bagGraspVNS = graspVNS.run(problemInstance.maxCapacity, problemInstance.packages, move, m_dependencyGraph, 200, maxGraspIterations);
            bagGraspVNS->setTimestamp(m_timestamp);
            updateBestBag(bagGraspVNS);
            resultBag.push_back(std::move(bagGraspVNS));
        }
    }

    for (auto& bag : resultBag){
        bag->setSeed(m_seed);
    }

    return resultBag;
}

// =============================================================
// == Dependency Precomputation (unchanged)
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
