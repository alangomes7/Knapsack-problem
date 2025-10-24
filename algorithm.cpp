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

    std::shared_ptr<Bag> bestInitialBag; // always valid
    int bestBenefit = std::numeric_limits<int>::min();

    std::mutex bagMutex; // protects saveBag() and resultBag.push_back()

    auto updateBestBag = [&](const std::unique_ptr<Bag>& bag) {
        if (bag && bag->getBenefit() > bestBenefit) {
            bestBenefit = bag->getBenefit();
            bestInitialBag = std::make_shared<Bag>(*bag); // copy
        }
    };

    auto saveBag = [&](const std::unique_ptr<Bag>& bag) {
        std::vector<std::unique_ptr<Bag>> tempVec;
        tempVec.push_back(std::make_unique<Bag>(*bag));
        FileProcessor::saveData(tempVec, m_outputDir, m_inputFile, m_fileId);
    };

    // === Constructive Phase ===
    {
        auto bag = constructiveSolutions.randomBag(problemInstance.maxCapacity, problemInstance.packages);
        updateBestBag(bag);
        saveBag(bag);
        resultBag.push_back(std::move(bag));

        auto greedyBags = constructiveSolutions.greedyBag(problemInstance.maxCapacity, problemInstance.packages);
        for (auto& bag : greedyBags) {
            updateBestBag(bag);
            saveBag(bag);
            resultBag.push_back(std::move(bag));
        }

        auto randomGreedyBags = constructiveSolutions.randomGreedy(problemInstance.maxCapacity, problemInstance.packages);
        for (auto& bag : randomGreedyBags) {
            updateBestBag(bag);
            saveBag(bag);
            resultBag.push_back(std::move(bag));
        }
    }

    // === Improvement Phase ===
    {
        // ----------------------
        // Shared resources
        // ----------------------
        std::mutex bagMutex; // protects bestInitialBag, bestBenefit, resultBag
        std::shared_ptr<Bag> bestInitialBag; // always valid
        int bestBenefit = std::numeric_limits<int>::min();

        // ----------------------
        // Save function
        // ----------------------
        auto saveBag = [&](const std::unique_ptr<Bag>& bag) {
            std::vector<std::unique_ptr<Bag>> tempVec;
            tempVec.push_back(std::make_unique<Bag>(*bag));
            FileProcessor::saveData(tempVec, m_outputDir, m_inputFile, m_fileId);
        };

        // ----------------------
        // Ensure bestInitialBag is valid
        // ----------------------
        if (!bestInitialBag && !resultBag.empty()) {
            bestInitialBag = std::make_shared<Bag>(*resultBag.front());
        }

        // ----------------------
        // Update best bag safely
        // ----------------------
        auto updateBestBag = [&](const std::unique_ptr<Bag>& bag) {
            if (!bag) return;
            std::lock_guard<std::mutex> lock(bagMutex);
            if (bag->getBenefit() > bestBenefit) {
                bestBenefit = bag->getBenefit();
                bestInitialBag = std::make_shared<Bag>(*bag);
            }
        };

        // ----------------------
        // Movements and iterations
        // ----------------------
        std::vector<SearchEngine::MovementType> moves = {
            SearchEngine::MovementType::ADD,
            SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
            SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2,
            SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1,
            SearchEngine::MovementType::EJECTION_CHAIN
        };
        int maxGraspIterations = 100;

        // ----------------------
        // VND (async task)
        // ----------------------
        auto vndFuture = std::async(std::launch::async, [&]() {
            std::mt19937 rng(m_generator());
            VND vnd(m_maxTime, rng());

            std::shared_ptr<Bag> inputBag;
            {
                std::lock_guard<std::mutex> lock(bagMutex);
                inputBag = bestInitialBag; // copy shared_ptr safely
            }

            auto vndBag = vnd.run(problemInstance.maxCapacity, inputBag.get(),
                                problemInstance.packages, m_dependencyGraph);
            vndBag->setTimestamp(m_timestamp);

            // Update shared resources
            {
                std::lock_guard<std::mutex> lock(bagMutex);
                saveBag(vndBag);
                resultBag.push_back(std::move(vndBag));
                if (resultBag.back() && resultBag.back()->getBenefit() > bestBenefit) {
                    bestBenefit = resultBag.back()->getBenefit();
                    bestInitialBag = std::make_shared<Bag>(*resultBag.back());
                }
            }
        });

        // ----------------------
        // Thread pool for VNS, GRASP, GRASP_VNS
        // ----------------------
        std::vector<std::thread> allThreads;

        // VNS depends on VND
        allThreads.emplace_back([&]() {
            vndFuture.get(); // wait for VND

            std::mt19937 rng(m_generator());
            VNS vns(m_maxTime, rng());

            std::shared_ptr<Bag> inputBag;
            {
                std::lock_guard<std::mutex> lock(bagMutex);
                inputBag = bestInitialBag;
            }

            auto vnsBag = vns.run(problemInstance.maxCapacity, inputBag.get(),
                                problemInstance.packages, m_dependencyGraph);
            vnsBag->setTimestamp(m_timestamp);

            {
                std::lock_guard<std::mutex> lock(bagMutex);
                saveBag(vnsBag);
                resultBag.push_back(std::move(vnsBag));
                if (resultBag.back() && resultBag.back()->getBenefit() > bestBenefit) {
                    bestBenefit = resultBag.back()->getBenefit();
                    bestInitialBag = std::make_shared<Bag>(*resultBag.back());
                }
            }
        });

        // GRASP (fully parallel)
        for (auto move : moves) {
            allThreads.emplace_back([&, move]() {
                std::mt19937 rng(m_generator());
                GRASP graspThread(m_maxTime, rng(), static_cast<int>(problemInstance.packages.size() / 3));

                auto bag = graspThread.run(problemInstance.maxCapacity, problemInstance.packages,
                                        move, m_dependencyGraph, 200, maxGraspIterations);
                bag->setTimestamp(m_timestamp);

                updateBestBag(bag); // thread-safe
                {
                    std::lock_guard<std::mutex> lock(bagMutex);
                    saveBag(bag);
                    resultBag.push_back(std::move(bag));
                }
            });
        }

        // GRASP_VNS (fully parallel)
        for (auto move : moves) {
            allThreads.emplace_back([&, move]() {
                std::mt19937 rng(m_generator());
                GRASP_VNS graspVnsThread(m_maxTime, rng(), static_cast<int>(problemInstance.packages.size() / 3), -1);

                auto bag = graspVnsThread.run(problemInstance.maxCapacity, problemInstance.packages,
                                            move, m_dependencyGraph, 200, maxGraspIterations);
                bag->setTimestamp(m_timestamp);

                updateBestBag(bag); // thread-safe
                {
                    std::lock_guard<std::mutex> lock(bagMutex);
                    saveBag(bag);
                    resultBag.push_back(std::move(bag));
                }
            });
        }

        // ----------------------
        // Wait for all threads
        // ----------------------
        for (auto& t : allThreads) t.join();

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