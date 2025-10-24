#include "grasp_helper.h"
#include "algorithm.h" // For Algorithm::ALGORITHM_TYPE
#include <limits>
#include <numeric>     // For std::iota
#include <algorithm>   // For std::sort, std::nth_element
#include <random>      // For std::uniform_real_distribution

namespace GraspHelper {

double calculateGreedyScore(const Package* pkg, const Bag& bag,
                            const std::vector<const Dependency*>& dependencies)
{
    int addedSize = 0;
    for (const Dependency* dep : dependencies) {
        if (bag.getDependencies().count(dep) == 0) {
            addedSize += dep->getSize();
        }
    }

    if (addedSize <= 0) {
        // Prioritize packages that add no new dependencies
        return std::numeric_limits<double>::infinity();
    }

    double benefitRatio = static_cast<double>(pkg->getBenefit()) / static_cast<double>(addedSize);
    double normalizedBenefit = static_cast<double>(pkg->getBenefit()) / 1000.0;
    
    // Weighted sum: 70% benefit/size ratio, 30% raw benefit
    return 0.7 * benefitRatio + 0.3 * normalizedBenefit;
}


std::unique_ptr<Bag> constructionPhaseFast(
    int bagSize,
    const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    SearchEngine& searchEngine,
    std::vector<std::pair<Package*, double>>& candidateScoresBuffer,
    std::vector<Package*>& rclBuffer,
    int rclSize,
    double alpha,
    double& alpha_random_out)
{
    auto bag = std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::GRASP, "construction");
    std::mt19937& rng = searchEngine.getRandomGenerator();

    const size_t n = allPackages.size();
    std::vector<char> used(n, 0); // Use a mask to track used packages
    size_t remaining = n;

    // Reserve buffers
    candidateScoresBuffer.clear();
    candidateScoresBuffer.reserve(std::min<size_t>(1024, n));
    rclBuffer.clear();
    rclBuffer.reserve(std::min<size_t>(std::max(1, rclSize), n));

    while (remaining > 0) {
        candidateScoresBuffer.clear();

        // 1. Evaluate candidates (only those not used)
        for (size_t idx = 0; idx < n; ++idx) {
            if (used[idx]) continue;
            Package* pkg = allPackages[idx];
            auto it = dependencyGraph.find(pkg);
            if (it == dependencyGraph.end()) continue;
            const auto& deps = it->second;
            if (bag->canAddPackage(*pkg, bagSize, deps)) {
                double score = calculateGreedyScore(pkg, *bag, deps);
                candidateScoresBuffer.emplace_back(pkg, score);
            }
        }

        if (candidateScoresBuffer.empty()) break; // No more valid candidates

        // 2. Build RCL
        int k = std::min<int>(rclSize, static_cast<int>(candidateScoresBuffer.size()));
        if (static_cast<int>(candidateScoresBuffer.size()) > k) {
            // Find the k-th best score
            auto nth = candidateScoresBuffer.begin() + k;
            std::nth_element(candidateScoresBuffer.begin(), nth, candidateScoresBuffer.end(),
                             [](const auto& a, const auto& b){ return a.second > b.second; });
            // Sort just the top k
            std::sort(candidateScoresBuffer.begin(), nth,
                      [](const auto& a, const auto& b){ return a.second > b.second; });
        } else {
            std::sort(candidateScoresBuffer.begin(), candidateScoresBuffer.end(),
                      [](const auto& a, const auto& b){ return a.second > b.second; });
        }

        double bestScore = candidateScoresBuffer.front().second;
        double worstScore = candidateScoresBuffer[std::min<size_t>(candidateScoresBuffer.size() - 1, k - 1)].second;
        
        // Update random alpha if needed
        if (alpha < 0) {
            alpha_random_out = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        }
        double threshold = bestScore - alpha_random_out * (bestScore - worstScore);

        rclBuffer.clear();
        for (size_t i = 0; i < candidateScoresBuffer.size() && static_cast<int>(rclBuffer.size()) < rclSize; ++i) {
            if (candidateScoresBuffer[i].second >= threshold) {
                rclBuffer.push_back(candidateScoresBuffer[i].first);
            } else {
                break; // List is sorted, no more candidates
            }
        }

        if (rclBuffer.empty()) break; // Should not happen if candidates exist, but safe_guard

        // 3. Select from RCL
        std::uniform_int_distribution<size_t> dist(0, rclBuffer.size() - 1);
        Package* chosen = rclBuffer[dist(rng)];

        // 4. Add chosen package
        auto depsIt = dependencyGraph.find(chosen);
        const std::vector<const Dependency*> emptyDeps;
        const std::vector<const Dependency*>& deps = (depsIt != dependencyGraph.end()) ? depsIt->second : emptyDeps;
        if (bag->canAddPackage(*chosen, bagSize, deps)) {
            bag->addPackage(*chosen, deps);
        }

        // Mark as used
        for (size_t idx = 0; idx < n; ++idx) {
            if (!used[idx] && allPackages[idx] == chosen) {
                used[idx] = 1;
                --remaining;
                break;
            }
        }
    }
    return bag;
}

} // namespace GraspHelper