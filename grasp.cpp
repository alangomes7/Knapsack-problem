#include "grasp.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <limits>
#include <memory>
#include <functional>
#include <algorithm>
#include <random>
#include <iostream>
#include <numeric>
#include <cmath>

// ===============================================================
// Constructors
// ===============================================================

GRASP::GRASP(double maxTime, double alpha, int rclSize)
    : m_maxTime(maxTime), m_alpha(std::clamp(alpha, 0.0, 1.0)), 
      m_rclSize(std::max(1, rclSize)), m_searchEngine(0) {}

GRASP::GRASP(double maxTime, unsigned int seed, double alpha, int rclSize)
    : m_maxTime(maxTime), m_alpha(std::clamp(alpha, 0.0, 1.0)),
      m_rclSize(std::max(1, rclSize)), m_searchEngine(seed) {}

// ===============================================================
// Main GRASP loop (multi-threaded)
// ===============================================================

Bag* GRASP::run(int bagSize,
                const std::vector<Bag*>& initialBags,
                const std::vector<Package*>& allPackages,
                SearchEngine::MovementType moveType,
                Algorithm::LOCAL_SEARCH localSearchMethod,
                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                int maxLS_IterationsWithoutImprovement,
                int maxLS_Iterations)
{
    if (allPackages.empty()) {
        return new Bag(Algorithm::ALGORITHM_TYPE::NONE, "0");
    }

    auto start = std::chrono::steady_clock::now();
    
    // FIXED: Use proper duration cast to get the correct time_point type
    auto deadline = start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(m_maxTime));

    std::unique_ptr<Bag> bestBagOverall = nullptr;
    std::mutex bestBagMutex;

    // Initialize with the best of initial solutions if any
    if (!initialBags.empty()) {
        auto bestInitial = std::max_element(
            initialBags.begin(), initialBags.end(),
            [](const Bag* a, const Bag* b) { 
                return a->getBenefit() < b->getBenefit(); 
            });
        bestBagOverall = std::make_unique<Bag>(**bestInitial);
    } else {
        // Create empty bag as fallback
        bestBagOverall = std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::GRASP, "0");
    }

    // Determine optimal number of threads
    unsigned int numThreads = std::max(1u, std::thread::hardware_concurrency());
    numThreads = std::min(numThreads, static_cast<unsigned int>(allPackages.size() / 100 + 1));
    
    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    std::cout << "GRASP starting with " << numThreads << " threads, alpha=" << m_alpha 
              << ", RCL size=" << m_rclSize << std::endl;

    // Create worker contexts and launch threads
    for (unsigned int i = 0; i < numThreads; ++i) {
        WorkerContext context{
            bagSize,
            &allPackages,
            moveType,
            localSearchMethod,
            &dependencyGraph,
            maxLS_IterationsWithoutImprovement,
            maxLS_Iterations,
            deadline,  // Now the correct type
            &bestBagOverall,
            &bestBagMutex
        };
        
        workers.emplace_back(&GRASP::graspWorker, this, std::move(context));
    }

    // Wait for all threads to complete
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    // Set final bag properties
    bestBagOverall->setAlgorithmTime(elapsed.count());
    bestBagOverall->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::GRASP);
    bestBagOverall->setLocalSearch(Algorithm::LOCAL_SEARCH::NONE);
    bestBagOverall->setMovementType(moveType);

    std::cout << "GRASP completed: " << m_totalIterations << " iterations, " 
              << m_improvements << " improvements" << std::endl;

    return bestBagOverall.release();
}

// ===============================================================
// Worker thread execution - SIMPLIFIED SIGNATURE
// ===============================================================

void GRASP::graspWorker(WorkerContext context) {
    SearchEngine localEngine(getThreadSeed());
    int localIterations = 0;
    int localImprovements = 0;

    while (std::chrono::steady_clock::now() < context.deadline) {
        ++localIterations;

        // --- Stage 1: Construction ---
        std::unique_ptr<Bag> currentBag = constructionPhase(
            context.bagSize, *context.allPackages, *context.dependencyGraph, localEngine);

        // --- Stage 2: Local Search ---
        localSearchPhase(localEngine, *currentBag, context.bagSize, *context.allPackages, 
                         context.moveType, context.localSearchMethod, *context.dependencyGraph,
                         context.maxLS_IterationsWithoutImprovement, 
                         context.maxLS_Iterations, context.deadline);

        // --- Stage 3: Update Best Solution ---
        std::lock_guard<std::mutex> lock(*context.bestBagMutex);
        if (currentBag->getBenefit() > (*context.bestBagOverall)->getBenefit()) {
            *context.bestBagOverall = std::move(currentBag);
            ++localImprovements;
        }
    }

    // Update global statistics
    m_totalIterations += localIterations;
    m_improvements += localImprovements;
    (*context.bestBagOverall)->setMetaheuristicParameters(
        "Alpha: " + std::to_string(m_alpha) + 
        " | Improvements: " + std::to_string(m_improvements) +
        " | RCL size: " + std::to_string(m_rclSize) +
        " | Total iterations: " + std::to_string(m_totalIterations)
    );

}

// ===============================================================
// Enhanced Construction Phase
// ===============================================================

std::unique_ptr<Bag> GRASP::constructionPhase(
    int bagSize,
    const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    SearchEngine& searchEngine)
{
    auto bag = std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::GRASP, "construction");
    std::vector<Package*> candidates = allPackages;
    
    std::mt19937& rng = searchEngine.getRandomGenerator();
    std::shuffle(candidates.begin(), candidates.end(), rng);

    // Pre-compute candidate scores for efficiency
    std::vector<std::pair<Package*, double>> candidateScores;
    candidateScores.reserve(candidates.size());

    while (!candidates.empty()) {
        candidateScores.clear();
        
        // Evaluate all candidates
        for (Package* pkg : candidates) {
            if (bag->canAddPackage(*pkg, bagSize, dependencyGraph.at(pkg))) {
                double score = calculateGreedyScore(pkg, *bag, dependencyGraph.at(pkg));
                candidateScores.emplace_back(pkg, score);
            }
        }

        if (candidateScores.empty()) break;

        // Sort by score (descending)
        std::sort(candidateScores.begin(), candidateScores.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        // Build Restricted Candidate List (RCL)
        double bestScore = candidateScores.front().second;
        double worstScore = candidateScores.back().second;
        double threshold = bestScore - m_alpha * (bestScore - worstScore);

        std::vector<Package*> RCL;
        RCL.reserve(std::min(m_rclSize, static_cast<int>(candidateScores.size())));
        
        for (const auto& [pkg, score] : candidateScores) {
            if (score >= threshold && RCL.size() < m_rclSize) {
                RCL.push_back(pkg);
            }
        }

        if (RCL.empty()) break;

        // Random selection from RCL
        std::uniform_int_distribution<size_t> dist(0, RCL.size() - 1);
        Package* chosen = RCL[dist(rng)];
        
        // Add chosen package to bag
        bag->addPackage(*chosen, dependencyGraph.at(chosen));

        // Remove chosen package from candidates efficiently
        candidates.erase(std::remove(candidates.begin(), candidates.end(), chosen), candidates.end());
    }

    return bag;
}

// ===============================================================
// Improved Greedy Score Calculation
// ===============================================================

double GRASP::calculateGreedyScore(const Package* pkg, const Bag& bag, 
                                  const std::vector<const Dependency*>& dependencies) const
{
    // Calculate the actual size this package would add
    int addedSize = 0;
    for (const Dependency* dep : dependencies) {
        if (bag.getDependencies().count(dep) == 0) {
            addedSize += dep->getSize();
        }
    }

    if (addedSize <= 0) {
        // Package adds no new dependencies - highly desirable
        return std::numeric_limits<double>::max();
    }

    // Combined metric: benefit per unit size + absolute benefit consideration
    double benefitRatio = static_cast<double>(pkg->getBenefit()) / addedSize;
    double normalizedBenefit = static_cast<double>(pkg->getBenefit()) / 1000.0;
    
    // Weighted combination
    return 0.7 * benefitRatio + 0.3 * normalizedBenefit;
}

// ===============================================================
// Enhanced Local Search Phase
// ===============================================================

void GRASP::localSearchPhase(
    SearchEngine& searchEngine,
    Bag& bag,
    int bagSize,
    const std::vector<Package*>& allPackages,
    SearchEngine::MovementType moveType,
    Algorithm::LOCAL_SEARCH localSearchMethod,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxLS_IterationsWithoutImprovement,
    int maxLS_Iterations,
    const std::chrono::steady_clock::time_point& deadline)
{
    // Early exit if bag is already full with high benefit
    if (bag.getBenefit() > 0 && bag.getSize() >= bagSize * 0.95) {
        return;
    }

    searchEngine.localSearch(
        bag, bagSize, allPackages, moveType,
        localSearchMethod, dependencyGraph,
        maxLS_IterationsWithoutImprovement,
        maxLS_Iterations, deadline);
}

// ===============================================================
// Helper Methods
// ===============================================================

unsigned int GRASP::getThreadSeed() const {
    return static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count()) ^
        std::hash<std::thread::id>()(std::this_thread::get_id()) ^
        (m_seedCounter.fetch_add(1, std::memory_order_relaxed));
}