#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "search_engine.h"

/**
 * @brief Provides common helper functions for GRASP-based algorithms.
 */
namespace GraspHelper {

    /**
     * @brief Calculates the greedy score for a package.
     * @param pkg The package to evaluate.
     * @param bag The current bag (to check existing dependencies).
     * @param dependencies The dependencies of the package.
     * @return The greedy score.
     */
    double calculateGreedyScore(
        const Package* pkg,
        const Bag& bag,
        const std::vector<const Dependency*>& dependencies);

    /**
     * @brief Performs the fast, randomized greedy construction phase of GRASP.
     *
     * @param bagSize Maximum bag capacity.
     * @param allPackages List of all available packages.
     * @param dependencyGraph Package dependency graph.
     * @param searchEngine A thread-local search engine for its RNG.
     * @param candidateScoresBuffer A thread-local buffer to reuse.
     * @param rclBuffer A thread-local buffer to reuse.
     * @param rclSize The size of the Restricted Candidate List.
     * @param alpha The alpha parameter controlling RCL greediness.
     * - $\alpha = 0$: Purely greedy (RCL = best candidate).
     * - $\alpha = 1$: Purely random (RCL = all valid candidates).
     * - $0 < \alpha < 1$: Semi-greedy (RCL = top candidates).
     * - $\alpha < 0$: Use a new random alpha $\in [0, 1]$ for each iteration.
     * @param alpha_random_out (Out) Reference to store the actual alpha value used
     * in this construction (especially when $\alpha < 0$).
     * @return A unique pointer to the newly constructed Bag.
     */
    std::unique_ptr<Bag> constructionPhaseFast(
        int bagSize,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        SearchEngine& searchEngine,
        std::vector<std::pair<Package*, double>>& candidateScoresBuffer,
        std::vector<Package*>& rclBuffer,
        int rclSize,
        double alpha,
        double& alpha_random_out);

} // namespace GraspHelper