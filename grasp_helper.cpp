#include "grasp_helper.h"
#include <algorithm>
#include <limits>

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

    if (addedSize <= 0) return std::numeric_limits<double>::infinity();

    double benefitRatio = static_cast<double>(pkg->getBenefit()) / static_cast<double>(addedSize);
    double normalizedBenefit = static_cast<double>(pkg->getBenefit()) / 1000.0;
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
    std::vector<char> used(n, 0);
    size_t remaining = n;

    // Reserve buffers once
    candidateScoresBuffer.reserve(std::min<size_t>(1024, n));
    rclBuffer.reserve(std::min<size_t>(std::max(1, rclSize), n));

    // Precompute dependency pointers
    std::vector<const std::vector<const Dependency*>*> depsPtrs(n, nullptr);
    for (size_t i = 0; i < n; ++i) {
        auto it = dependencyGraph.find(allPackages[i]);
        depsPtrs[i] = (it != dependencyGraph.end()) ? &it->second : nullptr;
    }

    while (remaining > 0) {
        candidateScoresBuffer.clear();

        // Evaluate candidates
        for (size_t idx = 0; idx < n; ++idx) {
            if (used[idx]) continue;

            Package* pkg = allPackages[idx];
            const std::vector<const Dependency*>& deps = depsPtrs[idx] ? *depsPtrs[idx] : std::vector<const Dependency*>{};

            if (bag->canAddPackage(*pkg, bagSize, deps)) {
                candidateScoresBuffer.emplace_back(pkg, calculateGreedyScore(pkg, *bag, deps));
            }
        }

        if (candidateScoresBuffer.empty()) break;

        int k = std::min<int>(rclSize, static_cast<int>(candidateScoresBuffer.size()));

        if (k < static_cast<int>(candidateScoresBuffer.size())) {
            std::partial_sort(candidateScoresBuffer.begin(),
                              candidateScoresBuffer.begin() + k,
                              candidateScoresBuffer.end(),
                              [](const auto& a, const auto& b){ return a.second > b.second; });
        } else {
            std::sort(candidateScoresBuffer.begin(), candidateScoresBuffer.end(),
                      [](const auto& a, const auto& b){ return a.second > b.second; });
        }

        double bestScore = candidateScoresBuffer.front().second;
        double worstScore = candidateScoresBuffer[k-1].second;

        if (alpha < 0) {
            alpha_random_out = std::uniform_real_distribution<double>(0.0, 1.0)(rng);
        }
        double threshold = bestScore - alpha_random_out * (bestScore - worstScore);

        rclBuffer.clear();
        for (int i = 0; i < k; ++i) {
            if (candidateScoresBuffer[i].second >= threshold)
                rclBuffer.push_back(candidateScoresBuffer[i].first);
        }

        if (rclBuffer.empty()) break;

        // Select random from RCL
        std::uniform_int_distribution<size_t> dist(0, rclBuffer.size() - 1);
        Package* chosen = rclBuffer[dist(rng)];

        const std::vector<const Dependency*>& deps = depsPtrs[std::distance(allPackages.begin(),
                                                                         std::find(allPackages.begin(), allPackages.end(), chosen))] ? 
                                                   *depsPtrs[std::distance(allPackages.begin(),
                                                                            std::find(allPackages.begin(), allPackages.end(), chosen))] : std::vector<const Dependency*>{};

        if (bag->canAddPackage(*chosen, bagSize, deps)) {
            bag->addPackage(*chosen, deps);
        }

        used[std::distance(allPackages.begin(), std::find(allPackages.begin(), allPackages.end(), chosen))] = 1;
        --remaining;
    }

    return bag;
}

} // namespace GraspHelper
