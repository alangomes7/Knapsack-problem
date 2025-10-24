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
#include "fileprocessor.h"
#include <future>

// Replaced non-breaking spaces with regular spaces
Algorithm::Algorithm(double maxTime, unsigned int seed, const std::string &outputDir, const std::string &inputFile, const std::string &fileId)
    : m_maxTime(maxTime), m_generator(seed), m_outputDir(outputDir), m_inputFile(inputFile), m_fileId(fileId)
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

    std::mutex bagMutex;
    std::shared_ptr<Bag> bestInitialBag;
    int bestBenefit = std::numeric_limits<int>::min();

    auto updateBestBag = [&](const std::unique_ptr<Bag>& bag) {
        if (!bag) return;
        std::lock_guard<std::mutex> lock(bagMutex);
        if (bag->getBenefit() > bestBenefit) {
            bestBenefit = bag->getBenefit();
            bestInitialBag = std::make_shared<Bag>(*bag);
        }
    };

    // Saves a vector of bags
    auto saveBags = [&](std::vector<std::unique_ptr<Bag>>& bags) {
        if (bags.empty()) return;
        FileProcessor::saveData(bags, m_outputDir, m_inputFile, m_fileId);
    };

    auto saveBag = [&](const std::unique_ptr<Bag>& bag) {
        if (!bag) return;
        std::vector<std::unique_ptr<Bag>> tempVec;
        tempVec.push_back(std::make_unique<Bag>(*bag));
        saveBags(tempVec);
    };

    // === Constructive Phase ===
    {
        resultBag.push_back(constructiveSolutions.randomBag(problemInstance.maxCapacity, problemInstance.packages));

        for (auto& bag : constructiveSolutions.greedyBag(problemInstance.maxCapacity, problemInstance.packages))
            resultBag.push_back(std::move(bag));

        for (auto& bag : constructiveSolutions.randomGreedy(problemInstance.maxCapacity, problemInstance.packages))
            resultBag.push_back(std::move(bag));

        for (auto& bag : resultBag) updateBestBag(bag);
        saveBags(resultBag);
    }

    // === Improvement Phase ===
    if (!bestInitialBag && !resultBag.empty())
        bestInitialBag = std::make_shared<Bag>(*resultBag.front());

    std::vector<SearchEngine::MovementType> moves = {
        SearchEngine::MovementType::ADD,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2,
        SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1,
        SearchEngine::MovementType::EJECTION_CHAIN
    };
    int maxGraspIterations = 100;

    // --- STEP 1: VND + VNS (parallel threads) ---
    {
        auto runMeta = [&](auto& meta) {
            std::mt19937 rng(m_generator());
            auto bag = meta.run(problemInstance.maxCapacity, bestInitialBag.get(),
                                problemInstance.packages, m_dependencyGraph);
            bag->setTimestamp(m_timestamp);
            updateBestBag(bag);
            saveBag(bag);

            std::lock_guard<std::mutex> lock(bagMutex);
            resultBag.push_back(std::move(bag));
        };

        std::thread vndThread([&]() { VND vnd(m_maxTime, m_generator()); runMeta(vnd); });
        std::thread vnsThread([&]() { VNS vns(m_maxTime, m_generator()); runMeta(vns); });

        vndThread.join();
        vnsThread.join();
    }

    // --- STEP 2: GRASP and GRASP_VNS (parallel internally per movement) ---
    {
        auto runGrasp = [&]<typename Constructor>() {
            std::vector<std::thread> threads;
            std::vector<std::unique_ptr<Bag>> groupBags;
            std::mutex groupMutex;

            for (auto move : moves) {
                threads.emplace_back([&, move]() {
                    std::mt19937 rng(m_generator());

                    Constructor graspThread(m_maxTime, rng(), static_cast<int>(problemInstance.packages.size() / 3), -1);

                    auto bag = graspThread.run(problemInstance.maxCapacity, problemInstance.packages,
                                            move, m_dependencyGraph, 200, maxGraspIterations);
                    bag->setTimestamp(m_timestamp);

                    updateBestBag(bag); // thread-safe
                    saveBag(bag);

                    std::lock_guard<std::mutex> lock(groupMutex);
                    groupBags.push_back(std::move(bag));
                });
            }

            for (auto& t : threads) t.join();

            // Save and append results
            std::lock_guard<std::mutex> lock(bagMutex);
            for (auto& b : groupBags) resultBag.push_back(std::move(b));
        };

        runGrasp.template operator()<GRASP>();
        runGrasp.template operator()<GRASP_VNS>();
    }

    return resultBag;
}

// =============================================================
// == String Conversions
// =============================================================

std::string Algorithm::toString(Algorithm::ALGORITHM_TYPE algorithm) const {
    // Replaced non-breaking spaces with regular spaces
    switch (algorithm) {
        case Algorithm::ALGORITHM_TYPE::RANDOM: return "RANDOM";
        case Algorithm::ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT: return "GREEDY_PACKAGE-BENEFIT";
        case Algorithm::ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT_RATIO: return "GREEDY_PACKAGE-BENEFIT_RATIO";
        case Algorithm::ALGORITHM_TYPE::GREEDY_PACKAGE_SIZE: return "GREEDY_PACKAGE-SIZE";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_BENEFIT: return "RANDOM_GREEDY_PACKAGE-BENEFIT";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_BENEFIT_RATIO: return "RANDOM_GREEDY_PACKAGE-BENEFIT_RATIO";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_SIZE: return "RANDOM_GREEDY_PACKAGE-SIZE";
        case Algorithm::ALGORITHM_TYPE::VND: return "VND";
        case Algorithm::ALGORITHM_TYPE::VNS: return "VNS";
        case Algorithm::ALGORITHM_TYPE::GRASP: return "GRASP";
        case Algorithm::ALGORITHM_TYPE::GRASP_VNS: return "GRASP_VNS";
        default: return "NONE";
    }
}

std::string Algorithm::toString(Algorithm::LOCAL_SEARCH localSearch) const {
    // Replaced non-breaking spaces with regular spaces
    switch (localSearch) {
        case Algorithm::LOCAL_SEARCH::FIRST_IMPROVEMENT: return "First Improvement";
        case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT: return "Best Improvement";
        case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT: return "Random Improvement";
        case Algorithm::LOCAL_SEARCH::NONE: return "None";
        default: return "VNS";
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