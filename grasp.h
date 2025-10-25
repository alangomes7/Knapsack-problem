#pragma once

#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <limits>
#include <memory>
#include <algorithm>
#include <random>
#include <iostream>
#include <numeric>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <string>

#include "algorithm.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "search_engine.h"

// WorkerContext reused to pass args into worker thread
struct WorkerContext {
    int bagSize = 0;
    const std::vector<Package*>* allPackages = nullptr;
    SEARCH_ENGINE::MovementType moveType{};
    const std::unordered_map<const Package*, std::vector<const Dependency*>>* dependencyGraph = nullptr;
    int maxLS_IterationsWithoutImprovement = 0;
    int max_Iterations = 0;
    std::chrono::steady_clock::time_point deadline{};
    std::unique_ptr<Bag>* bestBagOverall = nullptr;
    std::mutex* bestBagMutex = nullptr;
};

class GRASP {
public:
    GRASP(double maxTime, unsigned int seed, int rclSize, double alpha = -1);

    std::unique_ptr<Bag> run(
        int bagSize,
        const std::vector<Package*>& allPackages,
        SEARCH_ENGINE::MovementType moveType,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        int maxLS_IterationsWithoutImprovement,
        int max_Iterations);

private:
    // worker and phases
    void graspWorker(WorkerContext ctx);
    std::unique_ptr<Bag> constructionPhaseFast(
        int bagSize,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        SearchEngine& searchEngine,
        std::vector<std::pair<Package*, double>>& candidateScoresBuffer,
        std::vector<Package*>& rclBuffer);
    double calculateGreedyScore(const Package* pkg, const Bag& bag,
                                const std::vector<const Dependency*>& dependencies) const;
    void localSearchPhase(
        SearchEngine& searchEngine,
        Bag& bag,
        int bagSize,
        const std::vector<Package*>& allPackages,
        SEARCH_ENGINE::MovementType moveType,
        ALGORITHM::LOCAL_SEARCH localSearchMethod,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        int maxLS_IterationsWithoutImprovement,
        int maxLS_Iterations,
        const std::chrono::steady_clock::time_point& deadline);

private:
    const double m_maxTime;
    const double m_alpha;
    double m_alpha_random;
    const int m_rclSize;
    SearchEngine m_searchEngine;

    std::atomic<long long> m_totalIterations{0};
    std::atomic<long long> m_improvements{0};
    std::atomic<uint32_t> m_seedCounter{0};
};
