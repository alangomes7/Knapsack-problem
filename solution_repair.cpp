#include "solution_repair.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "random_provider.h"

#include <algorithm>
#include <unordered_set>
#include <limits>
#include <cmath>

namespace SOLUTION_REPAIR {

// --- Internal helpers (anonymous namespace) ---
namespace {

double computeEfficiency(const Package* pkg) noexcept {
    double weight = pkg->getDependenciesSize();
    if (weight <= 0) return std::numeric_limits<double>::max();
    return static_cast<double>(pkg->getBenefit()) / weight;
}

double computeRemovalScore(const Package* pkg, int dependents, double efficiency) noexcept {
    return efficiency + dependents + 1.0 / (pkg->getBenefit() + 1.0);
}

bool smartRemoval(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    std::unordered_map<const Package*, std::unordered_set<const Package*>> dependentsMap;
    for (const Package* pkg : packagesInBag) {
        auto it = dependencyGraph.find(pkg);
        if (it != dependencyGraph.end()) {
            for (const Dependency* dep : it->second) {
                for (const auto& [pkgId, depPkg] : dep->getAssociatedPackages()) {
                    if (depPkg && packagesInBag.count(depPkg) > 0)
                        dependentsMap[depPkg].insert(pkg);
                }
            }
        }
    }

    const Package* bestCandidate = nullptr;
    double bestScore = std::numeric_limits<double>::max();
    for (const Package* pkg : packagesInBag) {
        double eff = computeEfficiency(pkg);
        int dependentsCount = dependentsMap.count(pkg) ? dependentsMap.at(pkg).size() : 0;
        double score = computeRemovalScore(pkg, dependentsCount, eff);
        if (score < bestScore) {
            bestScore = score;
            bestCandidate = pkg;
        }
    }

    if (bestCandidate) {
        auto it = dependencyGraph.find(bestCandidate);
        bag.removePackage(*bestCandidate, it != dependencyGraph.end() ? it->second : std::vector<const Dependency*>{});
        return true;
    }

    return false;
}

bool probabilisticGreedyRemoval(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
    // Removed 'RANDOM_PROVIDER& random'
)
{
    const auto& pkgs = bag.getPackages();
    if (pkgs.empty()) return false;

    std::vector<std::pair<const Package*, double>> scored;
    scored.reserve(pkgs.size());

    for (auto* pkg : pkgs)
        scored.emplace_back(pkg, 1.0 / (computeEfficiency(pkg) + 1.0));

    double total = 0.0;
    for (auto& [p, s] : scored) total += s;

    double r = RANDOM_PROVIDER::getDouble(0.0, total);

    double acc = 0.0;
    const Package* selected = nullptr;
    for (auto& [p, s] : scored) {
        acc += s;
        if (r <= acc) { selected = p; break; }
    }

    if (!selected && !scored.empty()) selected = scored.back().first;

    if (selected) {
        auto it = dependencyGraph.find(selected);
        bag.removePackage(*selected, it != dependencyGraph.end() ? it->second : std::vector<const Dependency*>{});
        return true;
    }

    return false;
}

bool temperatureBiasedRemoval(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
    // Removed 'RANDOM_PROVIDER& random'
)
{
    double temperature = std::max(1.0, std::sqrt(bag.getSize() - maxCapacity));
    const auto& pkgs = bag.getPackages();
    if (pkgs.empty()) return false;

    const Package* chosen = nullptr;
    double bestScore = std::numeric_limits<double>::max();

    for (auto* pkg : pkgs) {
        double eff = computeEfficiency(pkg);
        double noise = RANDOM_PROVIDER::getDouble(0.8, 1.2);
        double score = eff * noise * (1.0 + temperature * 0.05);
        if (score < bestScore || RANDOM_PROVIDER::getDouble(0.0, 1.0) < std::exp(-score / temperature)) {
            bestScore = score;
            chosen = pkg;
        }
    }

    if (chosen) {
        auto it = dependencyGraph.find(chosen);
        bag.removePackage(*chosen, it != dependencyGraph.end() ? it->second : std::vector<const Dependency*>{});
        return true;
    }

    return false;
}

bool removeOnePackageWithStrategy(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    FEASIBILITY_STRATEGY strategy
)
{
    bag.setFeasibilityStrategy(strategy);
    switch (strategy) {
        case FEASIBILITY_STRATEGY::SMART:
            return smartRemoval(bag, maxCapacity, dependencyGraph);
        case FEASIBILITY_STRATEGY::TEMPERATURE_BIASED:
            return temperatureBiasedRemoval(bag, maxCapacity, dependencyGraph);
        case FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY:
            return probabilisticGreedyRemoval(bag, maxCapacity, dependencyGraph);
    }
    return false;
}

}

// --- Public run() function ---
bool repair(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
    // Removed 'RANDOM_PROVIDER& random'
)
{
    if (bag.getSize() <= maxCapacity)
        return true;

    std::vector<std::pair<FEASIBILITY_STRATEGY, Bag>> results;
    results.reserve(3);

    for (FEASIBILITY_STRATEGY strat : { FEASIBILITY_STRATEGY::SMART,
                                       FEASIBILITY_STRATEGY::TEMPERATURE_BIASED,
                                       FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY })
    {
        Bag testBag = bag;
        while (testBag.getSize() > maxCapacity) {
            // Removed 'random' from call
            if (!removeOnePackageWithStrategy(testBag, maxCapacity, dependencyGraph, strat))
                break;
        }
        if (testBag.getSize() <= maxCapacity)
            results.emplace_back(strat, std::move(testBag));
    }

    if (results.empty()) {
        // fallback: choose smallest infeasible
        Bag best = bag;
        for (FEASIBILITY_STRATEGY strat : { FEASIBILITY_STRATEGY::SMART,
                                            FEASIBILITY_STRATEGY::TEMPERATURE_BIASED,
                                            FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY })
        {
            Bag testBag = bag;
            while (testBag.getSize() > maxCapacity) {
                if (!removeOnePackageWithStrategy(testBag, maxCapacity, dependencyGraph, strat))
                    break;
            }
            if (testBag.getSize() < best.getSize())
                best = std::move(testBag);
        }
        bag = std::move(best);
        return false;
    }

    // pick feasible with max benefit
    auto bestIt = std::max_element(results.begin(), results.end(),
        [](const auto& a, const auto& b){ return a.second.getBenefit() < b.second.getBenefit(); });
    bag = bestIt->second;
    return true;
}

std::string toString(FEASIBILITY_STRATEGY feasibilityStrategy)
{
    switch (feasibilityStrategy)
    {
    case FEASIBILITY_STRATEGY::SMART:
        return "SMART";
    case FEASIBILITY_STRATEGY::TEMPERATURE_BIASED:
        return "TEMPERATURE_BIASED";
    default:
        return "PROBABILISTIC_GREEDY";
    }
}
}