#include "MetaheuristicHelper.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <unordered_set>
#include <memory>

MetaheuristicHelper::MetaheuristicHelper(unsigned int seed)
    : m_generator(seed), m_seed(seed) {}

int MetaheuristicHelper::getStrategyCount() const noexcept { return 3; }
int MetaheuristicHelper::getSeed() const { return m_seed; }

double MetaheuristicHelper::computeEfficiency(const Package* pkg) const noexcept {
    double weight = pkg->getDependenciesSize();
    if (weight <= 0) return std::numeric_limits<double>::max();
    return static_cast<double>(pkg->getBenefit()) / weight;
}

double MetaheuristicHelper::computeRemovalScore(const Package* pkg, int dependents, double eff) const noexcept {
    double dependentsPenalty = dependents;
    return eff + dependentsPenalty + (1.0 / (pkg->getBenefit() + 1.0));
}

bool MetaheuristicHelper::makeItFeasible(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (bag.getSize() <= maxCapacity)
        return true;

    // Test all three strategies and choose the best feasible result
    std::vector<std::pair<FeasibilityStrategy, Bag>> results;
    results.reserve(3);

    for (FeasibilityStrategy strat : {
             FeasibilityStrategy::SMART,
             FeasibilityStrategy::TEMPERATURE_BIASED,
             FeasibilityStrategy::PROBABILISTIC_GREEDY})
    {
        Bag testBag = bag; // copy original
        while (testBag.getSize() > maxCapacity) {
            if (!removeOnePackageWithStrategy(testBag, maxCapacity, dependencyGraph, strat))
                break;
        }
        results.emplace_back(strat, std::move(testBag));
            
    }

    // Choose the feasible bag with the best total benefit
    auto bestIt = std::max_element(results.begin(), results.end(),
        [](const auto& a, const auto& b) {
            return a.second.getBenefit() < b.second.getBenefit();
        });

    // Replace the input bag with the best feasible result
    bag = bestIt->second;
    return true;
}

bool MetaheuristicHelper::removeOnePackageWithStrategy(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    FeasibilityStrategy strategy)
{
    if (bag.getPackages().empty()) return false;

    switch (strategy) {
        case FeasibilityStrategy::SMART:
            return smartRemoval(bag, maxCapacity, dependencyGraph);
        case FeasibilityStrategy::TEMPERATURE_BIASED:
            return temperatureBiasedRemoval(bag, maxCapacity, dependencyGraph);
        case FeasibilityStrategy::PROBABILISTIC_GREEDY:
            return probabilisticGreedyRemoval(bag, maxCapacity, dependencyGraph);
        default:
            return smartRemoval(bag, maxCapacity, dependencyGraph);
    }
}

bool MetaheuristicHelper::smartRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    // Build dependency impact map
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

    // Find best candidate to remove
    const Package* bestCandidate = nullptr;
    double bestScore = std::numeric_limits<double>::max();

    for (const Package* pkg : packagesInBag) {
        int weight = pkg->getDependenciesSize();
        int benefit = pkg->getBenefit();
        double efficiency = (weight > 0) ? static_cast<double>(benefit) / weight :
                            (benefit <= 0 ? -1.0 : std::numeric_limits<double>::max());
        int dependentsCount = dependentsMap[pkg].size();
        double score = computeRemovalScore(pkg, dependentsCount, efficiency);
        if (score < bestScore) {
            bestScore = score;
            bestCandidate = pkg;
        }
    }

    if (bestCandidate) {
        auto it = dependencyGraph.find(bestCandidate);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*bestCandidate, deps);
        return true;
    }

    return false;
}

bool MetaheuristicHelper::probabilisticGreedyRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    auto& pkgs = bag.getPackages();
    if (pkgs.empty()) return false;

    std::vector<std::pair<const Package*, double>> scored;
    scored.reserve(pkgs.size());

    for (auto* pkg : pkgs)
        scored.emplace_back(pkg, 1.0 / (computeEfficiency(pkg) + 1.0));

    double total = 0.0;
    for (auto& [p, s] : scored) total += s;

    std::uniform_real_distribution<double> dist(0.0, total);
    double r = dist(m_generator);

    double acc = 0.0;
    const Package* selected = nullptr;
    for (auto& [p, s] : scored) {
        acc += s;
        if (r <= acc) { selected = p; break; }
    }

    if (!selected) selected = scored.back().first;

    auto it = dependencyGraph.find(selected);
    const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
    bag.removePackage(*selected, deps);
    return true;
}

bool MetaheuristicHelper::temperatureBiasedRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    double temperature = std::max(1.0, std::sqrt(bag.getSize() - maxCapacity));
    const auto& pkgs = bag.getPackages();
    if (pkgs.empty()) return false;

    const Package* chosen = nullptr;
    double bestScore = std::numeric_limits<double>::max();

    for (auto* pkg : pkgs) {
        double eff = computeEfficiency(pkg);
        double noise = randomNumberDouble(0.8, 1.2);
        double score = eff * noise * (1.0 + temperature * 0.05);
        if (score < bestScore || randomNumberDouble(0.0, 1.0) < std::exp(-score / temperature)) {
            bestScore = score;
            chosen = pkg;
        }
    }

    if (chosen) {
        auto it = dependencyGraph.find(chosen);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*chosen, deps);
        return true;
    }

    return false;
}

int MetaheuristicHelper::randomNumberInt(int min, int max) {
    if (min > max) return min;
    std::uniform_int_distribution<> dist(min, max);
    return dist(m_generator);
}

double MetaheuristicHelper::randomNumberDouble(double min, double max) {
    if (min > max) return min;
    std::uniform_real_distribution<double> dist(min, max);
    return dist(m_generator);
}