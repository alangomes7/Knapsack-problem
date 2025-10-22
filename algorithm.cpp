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
#include "SearchEngine.h"
#include "solution_repair.h"

// Replaced non-breaking spaces with regular spaces
Algorithm::Algorithm(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_generator(seed)
{
}

// =============================================================
// == Main Control: Executes all strategies (construct + improve)
// =============================================================

std::vector<std::unique_ptr<Bag>> Algorithm::run(const ProblemInstance& problemInstance, const std::string& timestamp)
{
    m_timestamp = timestamp;

    // Precompute dependencies once
    precomputeDependencyGraph(problemInstance.packages, problemInstance.dependencies);

    // Create the strategy manager (constructive phase)
    ConstructiveSolutions constructiveSolutions(m_maxTime, m_generator, m_dependencyGraph, m_timestamp);

    // Vector now holds smart pointers
    std::vector<std::unique_ptr<Bag>> resultBag;
    resultBag.reserve(10);

    // === Constructive Phase ===
    // Use problemInstance members and std::move the resulting unique_ptr
    resultBag.push_back(constructiveSolutions.randomBag(problemInstance.maxCapacity, problemInstance.packages));
    
    // greedyBags and randomGreedyBags are now vectors of unique_ptr
    std::vector<std::unique_ptr<Bag>> greedyBags = constructiveSolutions.greedyBag(problemInstance.maxCapacity, problemInstance.packages);
    std::vector<std::unique_ptr<Bag>> randomGreedyBags = constructiveSolutions.randomGreedy(problemInstance.maxCapacity, problemInstance.packages);

    // We must std::move unique_ptr elements from one vector to another
    resultBag.insert(resultBag.end(),
                     std::make_move_iterator(greedyBags.begin()),
                     std::make_move_iterator(greedyBags.end()));
    resultBag.insert(resultBag.end(),
                     std::make_move_iterator(randomGreedyBags.begin()),
                     std::make_move_iterator(randomGreedyBags.end()));

    // === Select best initial bag ===
    // Use a non-owning raw pointer for observation
    Bag* bestInitialBag = nullptr;
    int bestInitialBenefit = -1;
    // Iterate over the vector of unique_ptrs
    for (const auto& bag : resultBag) {
        if (bag->getBenefit() > bestInitialBenefit) {
            bestInitialBenefit = bag->getBenefit();
            bestInitialBag = bag.get(); // Get the raw pointer
        }
    }

    // === Improvement Phase (Metaheuristics) ===
    // if (bestInitialBag) {
    //     // VND
    //     VND vnd(m_maxTime, m_generator());
    //     // Note: VND::run would also need to be updated to return std::unique_ptr
    //     std::unique_ptr<Bag> vndBag = vnd.run(problemInstance.bagSize, bestInitialBag, problemInstance.packages, m_dependencyGraph);
    //     vndBag->setTimestamp(m_timestamp);
    //     // fileProcessor->saveData({ vndBag.get() }); // Assuming saveData wants raw pointers

    //     // VNS
    //     VNS vns(m_maxTime, m_generator());
    //     std::unique_ptr<Bag> vnsBag = vns.run(problemInstance.bagSize, bestInitialBag, problemInstance.packages, m_dependencyGraph);
    //     // fileProcessor->saveData({ vnsBag.get() });

    //     // GRASP
    //     GRASP grasp(m_maxTime, m_generator());
    //     // This would need a more complex update, as it takes a vector of Bags
    //     // You might need to pass a vector of raw pointers instead
    //     // Bag* graspBag = grasp.run(problemInstance.bagSize, /*...raw pointer vector...*/, problemInstance.packages, SearchEngine::MovementType::EJECTION_CHAIN,
    //     //                          Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT, m_dependencyGraph);
    //     // graspBag->setTimestamp(m_timestamp);
    //     // fileProcessor->saveData({ graspBag });
    //     
    //     resultBag.push_back(std::move(vndBag));
    //     resultBag.push_back(std::move(vnsBag));
    //     // resultBag.push_back(std::move(graspBag)); // if grasp returns unique_ptr
    // }

    return resultBag;
}

// =============================================================
// == String Conversions
// =============================================================

std::string Algorithm::toString(Algorithm::ALGORITHM_TYPE algorithm) const {
    // Replaced non-breaking spaces with regular spaces
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
    // Replaced non-breaking spaces with regular spaces
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
    // Replaced non-breaking spaces with regular spaces
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