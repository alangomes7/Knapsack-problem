#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "search_engine.h"

/**
 * @brief Provides common helper functions for VNS-based algorithms.
 */
namespace VnsHelper {

    /**
     * @brief Shakes the current solution to escape local optima.
     *
     * This function perturbs the solution by randomly removing 'k' packages
     * and randomly adding 'k' different packages, creating a new solution
     * in the k-th neighborhood.
     *
     * @param currentBag Current solution to shake.
     * @param k Neighborhood size (number of packages to remove/add).
     * @param allPackages Vector of all available packages.
     * @param bagSize Maximum bag capacity.
     * @param dependencyGraph Precomputed package dependency graph.
     * @return Unique pointer to the new, shaken solution.
     */
    std::unique_ptr<Bag> shake(
        const Bag& currentBag,
        int k,
        const std::vector<Package*>& allPackages,
        int bagSize,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    /**
     * @brief Runs the core VNS iterative loop.
     *
     * This function takes an initial solution and attempts to improve it by
     * systematically shaking it into different neighborhoods ($k=1$ to $k_{max}$)
     * and applying a local search to the shaken solution. If an improvement
     * is found, the search restarts from $k=1$.
     *
     * @param bestBag (In/Out) The solution to improve. This object will be
     * modified in-place to hold the best-found solution.
     * @param bagSize Maximum bag capacity.
     * @param allPackages Vector of all available packages.
     * @param dependencyGraph Package dependency graph.
     * @param searchEngine A thread-local search engine for local search.
     * @param maxLS_IterationsWithoutImprovement Max LS iterations without improvement.
     * @param maxLS_Iterations Max total LS iterations.
     * @param deadline The time limit for the algorithm.
     */
    void vnsLoop(
        Bag& bestBag,
        int bagSize,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        SearchEngine& searchEngine,
        int maxLS_IterationsWithoutImprovement,
        int maxLS_Iterations,
        const std::chrono::steady_clock::time_point& deadline);

} // namespace VnsHelper