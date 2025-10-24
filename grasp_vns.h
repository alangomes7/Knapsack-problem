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
 * It runs multiple threads, each performing randomized greedy construction
 * (GRASP) followed by a Variable Neighborhood Search (VNS) local optimization.
 */
class GRASP_VNS {
public:
    /**
     * @brief Construct a new GRASP_VNS solver.
     * @param maxTime Maximum execution time (in seconds)
     * @param seed Random seed
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
     * @param maxLS_IterationsWithoutImprovement Max iterations without improvement in LS
     * @param max_Iterations Max GRASP iterations
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

    // ---------------- Core Worker ----------------
    void graspWorker(WorkerContext ctx);

    // ---------------- Internal State ----------------
    double m_maxTime;                 ///< Maximum allowed runtime (seconds)
    double m_alpha;                   ///< Greedy-randomness control parameter
    double m_alpha_random;            ///< Randomized alpha (updated per iteration)
    int m_rclSize;                    ///< Restricted Candidate List size
    SearchEngine m_searchEngine;      ///< Base search engine for all threads

    std::atomic<long long> m_totalIterations{0}; ///< Total number of iterations
    std::atomic<long long> m_improvements{0};    ///< Total number of improvements
};

#endif // GRASP_VNS_H