#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <random>
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "search_engine.h"

namespace GraspHelper {

    double calculateGreedyScore(
        const Package* pkg,
        const Bag& bag,
        const std::vector<const Dependency*>& dependencies);

    /**
     * @brief Performs a fast, randomized GRASP construction phase.
     *
     * Optimizations:
     * - Reuses buffers
     * - Precomputes dependency vectors
     * - Partial sort for small RCL
     * - Minimized RNG calls
     *
     * @param bagSize Maximum bag capacity
     * @param allPackages List of all packages
     * @param dependencyGraph Map of package dependencies
     * @param searchEngine Thread-local search engine for RNG
     * @param candidateScoresBuffer Thread-local reusable buffer
     * @param rclBuffer Thread-local reusable buffer
     * @param rclSize RCL size
     * @param alpha Alpha parameter
     * @param alpha_random_out Actual alpha used (output)
     * @return Constructed bag
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
