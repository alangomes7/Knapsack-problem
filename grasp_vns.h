#ifndef GRASP_VNS_H
#define GRASP_VNS_H

#include "algorithm.h"
#include "search_engine.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// Forward declarations to avoid heavy includes
class Bag;
class Package;
class Dependency;

/**
 * @brief GRASP_VNS combines GRASP construction and VNS intensification phases.
 *
 * Each thread independently performs randomized greedy construction (GRASP)
 * followed by a Variable Neighborhood Search (VNS) improvement phase. 
 * Periodically, the best solution is synchronized across threads.
 */
class GRASP_VNS {
public:
    /**
     * @brief Construct a new GRASP_VNS solver.
     * @param maxTime Maximum execution time (in seconds)
     * @param seed Random seed for reproducibility
     * @param rclSize Restricted Candidate List size
     * @param alpha GRASP alpha parameter (0=greedy, 1=random, <0=randomized)
     */
    GRASP_VNS(double maxTime, unsigned int seed, int rclSize, double alpha);

    /**
     * @brief Run the GRASP + VNS metaheuristic.
     * @param bagSize Capacity of the knapsack
     * @param allPackages List of all packages
     * @param moveType Movement type for the local search
     * @param dependencyGraph Map of package dependencies
     * @param maxLS_IterationsWithoutImprovement Max iterations without improvement in local search
     * @param max_Iterations Maximum GRASP iterations
     * @return std::unique_ptr<Bag> The best solution found
     */
    std::unique_ptr<Bag> run(
        int bagSize,
        const std::vector<Package*>& allPackages,
        SearchEngine::MovementType moveType,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        int maxLS_IterationsWithoutImprovement,
        int max_Iterations);

private:
    // ---------------- Worker Context ----------------
    struct WorkerContext {
        int bagSize;
        const std::vector<Package*>* allPackages;
        SearchEngine::MovementType moveType;
        const std::unordered_map<const Package*, std::vector<const Dependency*>>* dependencyGraph;
        int maxLS_IterationsWithoutImprovement;
        int max_Iterations;
        std::chrono::steady_clock::time_point deadline;

        std::unique_ptr<Bag>* bestBagOverall;
        std::mutex* bestBagMutex;
    };

    /**
     * @brief Core worker executed by each thread.
     * Performs multiple GRASP + VNS iterations, updating the global best solution.
     * Uses thread-local buffers and lightweight synchronization.
     */
    void graspWorker(WorkerContext ctx);

    // ---------------- Internal Parameters ----------------
    double m_maxTime;                 ///< Maximum allowed runtime (seconds)
    double m_alpha;                   ///< GRASP alpha (balance between greediness and randomness)
    double m_alpha_random;            ///< Randomized alpha (used when < 0)
    int m_rclSize;                    ///< Restricted Candidate List size
    SearchEngine m_searchEngine;      ///< Base random engine (thread-local copies are used per worker)

    // ---------------- Statistics ----------------
    std::atomic<long long> m_totalIterations{0}; ///< Total number of iterations across threads
    std::atomic<long long> m_improvements{0};    ///< Total number of improvements across threads
};

#endif // GRASP_VNS_H