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
 * @brief Provides optimized helper functions for VNS-based algorithms.
 */
namespace VNS_HELPER {

    /**
     * @brief Shakes the current solution to escape local optima.
     *
     * Randomly removes 'k' packages and adds up to 'k' new packages.
     * Optimized to reuse vectors and minimize RNG calls.
     *
     * @param currentBag Current solution to shake.
     * @param k Neighborhood size.
     * @param allPackages All available packages.
     * @param bagSize Maximum bag capacity.
     * @param dependencyGraph Precomputed package dependency graph.
     * @param generator A reference to the random number generator to use.
     * @param tmpOutside (Optional) reusable buffer for packages outside the bag.
     * @return Unique pointer to the new shaken solution.
     */
    std::unique_ptr<Bag> shake(
        const Bag& currentBag,
        int k,
        const std::vector<Package*>& allPackages,
        int bagSize,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        std::mt19937& generator,
        std::vector<Package*>& tmpOutside
    );

    /**
     * @brief Runs the VNS iterative loop with optional parallel neighborhood evaluation.
     *
     * @param bestBag (In/Out) Best bag to improve.
     * @param bagSize Maximum bag capacity.
     * @param allPackages Vector of all available packages.
     * @param dependencyGraph Package dependency graph.
     * @param searchEngine Thread-local search engine.
     * @param maxLS_IterationsWithoutImprovement Max LS iterations without improvement.
     * @param maxLS_Iterations Max total LS iterations.
     * @param deadline Time limit.
     */
    void vnsLoop(
        Bag& bestBag,
        int bagSize,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        SearchEngine& searchEngine,
        int maxLS_IterationsWithoutImprovement,
        int maxLS_Iterations,
        const std::chrono::steady_clock::time_point& deadline
    );

} // namespace VNS_HELPER
