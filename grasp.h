#ifndef GRASP_H
#define GRASP_H

#include <vector>
#include <random>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <numeric>

#include "algorithm.h"
#include "searchEngine.h"

class Bag;
class Package;
class Dependency;

/**
 * @brief Enhanced GRASP (Greedy Randomized Adaptive Search Procedure) metaheuristic.
 */
class GRASP {
public:
    explicit GRASP(double maxTime, double alpha = 0.3, int rclSize = 10);
    GRASP(double maxTime, unsigned int seed, double alpha = 0.3, int rclSize = 10);
    
    ~GRASP() = default;
    GRASP(const GRASP&) = delete;
    GRASP& operator=(const GRASP&) = delete;

    Bag* run(int bagSize,
             const std::vector<Bag*>& initialBags,
             const std::vector<Package*>& allPackages,
             SearchEngine::MovementType moveType,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
             int maxLS_IterationsWithoutImprovement = 150,
             int maxLS_Iterations = 1500);

    // Configuration setters
    void setAlpha(double alpha) { m_alpha = std::clamp(alpha, 0.0, 1.0); }
    void setRCLSize(int size) { m_rclSize = std::max(1, size); }

private:
    // Worker thread context to avoid complex parameter passing
    struct WorkerContext {
        int bagSize;
        const std::vector<Package*>* allPackages;
        SearchEngine::MovementType moveType;
        Algorithm::LOCAL_SEARCH localSearchMethod;
        const std::unordered_map<const Package*, std::vector<const Dependency*>>* dependencyGraph;
        int maxLS_IterationsWithoutImprovement;
        int maxLS_Iterations;
        std::chrono::steady_clock::time_point deadline;  // Correct type
        std::unique_ptr<Bag>* bestBagOverall;
        std::mutex* bestBagMutex;
    };

    // --- Core GRASP stages ---
    std::unique_ptr<Bag> constructionPhase(
        int bagSize,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        SearchEngine& searchEngine);

    void localSearchPhase(
        SearchEngine& searchEngine,
        Bag& bag,
        int bagSize,
        const std::vector<Package*>& allPackages,
        SearchEngine::MovementType moveType,
        Algorithm::LOCAL_SEARCH localSearchMethod,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        int maxLS_IterationsWithoutImprovement,
        int maxLS_Iterations,
        const std::chrono::steady_clock::time_point& deadline);

    // Main worker function - simplified signature
    void graspWorker(WorkerContext context);

    // --- Helper methods ---
    double calculateGreedyScore(const Package* pkg, const Bag& bag, 
                               const std::vector<const Dependency*>& dependencies) const;
    unsigned int getThreadSeed() const;

private:
    const double m_maxTime;
    double m_alpha;  // RCL parameter [0,1]
    int m_rclSize;   // Maximum RCL size
    
    SearchEngine m_searchEngine;
    mutable std::atomic<unsigned int> m_seedCounter{0};
    
    // Statistics
    std::atomic<int> m_totalIterations{0};
    std::atomic<int> m_improvements{0};
};

#endif // GRASP_H