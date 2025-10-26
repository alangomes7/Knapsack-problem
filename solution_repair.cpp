#include "solution_repair.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <algorithm>
#include <unordered_set>
#include <limits>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

namespace SOLUTION_REPAIR {

// =====================================================================================
// toString for FEASIBILITY_STRATEGY
// =====================================================================================
std::string toString(FEASIBILITY_STRATEGY strategy)
{
    switch (strategy) {
        case FEASIBILITY_STRATEGY::SMART: return "SMART";
        case FEASIBILITY_STRATEGY::TEMPERATURE_BIASED: return "TEMPERATURE_BIASED";
        case FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY: return "PROBABILISTIC_GREEDY";
        default: return "UNKNOWN";
    }
}

// =====================================================================================
// Private Helper Functions
// =====================================================================================

static int computeBagSize(const Bag& bag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    std::unordered_set<const Dependency*> deps;
    for (const auto* pkg : bag.getPackages()) {
        auto it = dependencyGraph.find(pkg);
        if (it != dependencyGraph.end()) {
            for (auto* dep : it->second)
                deps.insert(dep);
        }
    }
    int total = 0;
    for (auto* d : deps)
        total += d->getSize();
    return total;
}

static bool isValid(const Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    std::unordered_map<const Dependency*, int> ref;
    int benefit = 0;

    for (const auto* pkg : bag.getPackages()) {
        if (!pkg) continue;
        benefit += pkg->getBenefit();
        auto it = dependencyGraph.find(pkg);
        if (it == dependencyGraph.end()) return false;
        for (auto* dep : it->second) ref[dep]++;
    }

    int size = 0;
    for (auto& kv : ref)
        if (kv.second > 0)
            size += kv.first->getSize();

    if (benefit != bag.getBenefit()) return false;
    if (size != bag.getSize()) return false;
    if (size > maxCapacity) return false;
    return true;
}

struct PackageScore {
    const Package* pkg = nullptr;
    int benefit = 0;
    int uniqueSize = 0;
    double efficiency = 0.0;
    double inefficiency = 0.0;
    double smartScore = 0.0;
};

static std::vector<PackageScore> scorePackages(
    const Bag& bag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    std::vector<PackageScore> scores;
    scores.reserve(bag.getPackages().size());
    const auto& refCounts = bag.getDependencyRefCount();

    for (const Package* pkg : bag.getPackages()) {
        PackageScore s;
        s.pkg = pkg;
        s.benefit = pkg->getBenefit();
        s.uniqueSize = 0;

        const auto& deps = dependencyGraph.at(pkg);
        for (const auto* dep : deps) {
            if (refCounts.at(dep) == 1) s.uniqueSize += dep->getSize();
        }

        s.efficiency = (s.uniqueSize > 0) ? static_cast<double>(s.benefit) / s.uniqueSize
                                          : ((s.benefit > 0) ? std::numeric_limits<double>::max() : 0.0);
        s.inefficiency = (s.benefit > 0) ? static_cast<double>(s.uniqueSize) / s.benefit
                                         : ((s.uniqueSize > 0) ? std::numeric_limits<double>::max() : 0.0);
        s.smartScore = (s.efficiency * 0.7) + (static_cast<double>(s.benefit) * 0.3);

        scores.push_back(s);
    }
    return scores;
}

static const Package* selectProbabilistic(const std::vector<PackageScore>& scores, std::mt19937& rng) {
    double totalInefficiency = 0.0;
    for (const auto& s : scores) totalInefficiency += s.inefficiency;

    if (totalInefficiency == 0.0) {
        std::uniform_int_distribution<int> dist_idx(0, scores.size() - 1);
        return scores[dist_idx(rng)].pkg;
    }

    std::uniform_real_distribution<double> dist_roll(0.0, totalInefficiency);
    double roll = dist_roll(rng);
    double currentSum = 0.0;
    for (const auto& s : scores) {
        currentSum += s.inefficiency;
        if (roll <= currentSum) return s.pkg;
    }
    return scores.back().pkg;
}

static const Package* selectTemperatureBiased(const std::vector<PackageScore>& scores, double temperature, std::mt19937& rng) {
    const Package* worstPkg = nullptr;
    double worstScore = std::numeric_limits<double>::max();

    for (const auto& s : scores) {
        std::uniform_real_distribution<double> dist_noise(1.0 - temperature, 1.0 + temperature);
        double noisyScore = s.smartScore * dist_noise(rng);
        if (noisyScore < worstScore) {
            worstScore = noisyScore;
            worstPkg = s.pkg;
        }
    }
    return worstPkg;
}

static bool fixWithStrategy(Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    FEASIBILITY_STRATEGY strategy,
    unsigned int seed)
{
    std::mt19937 rng(seed);

    int currentSize = computeBagSize(bag, dependencyGraph);
    double initialOver = static_cast<double>(currentSize - maxCapacity);

    while (currentSize > maxCapacity && !bag.getPackages().empty()) {
        auto scores = scorePackages(bag, dependencyGraph);
        if (scores.empty()) break;

        const Package* pkgToRemove = nullptr;
        switch (strategy) {
            case FEASIBILITY_STRATEGY::SMART: {
                pkgToRemove = std::min_element(scores.begin(), scores.end(),
                                               [](const PackageScore& a, const PackageScore& b) {
                                                   return a.smartScore < b.smartScore;
                                               })->pkg;
                break;
            }
            case FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY:
                pkgToRemove = selectProbabilistic(scores, rng);
                break;
            case FEASIBILITY_STRATEGY::TEMPERATURE_BIASED: {
                double temperature = std::max(0.0, (currentSize - maxCapacity) / initialOver);
                pkgToRemove = selectTemperatureBiased(scores, temperature, rng);
                break;
            }
        }

        if (pkgToRemove) bag.removePackage(*pkgToRemove, dependencyGraph.at(pkgToRemove));
        currentSize = computeBagSize(bag, dependencyGraph);
    }

    return (currentSize <= maxCapacity);
}

// =====================================================================================
// PUBLIC REPAIR FUNCTION
// =====================================================================================
bool repair(Bag& bag, int maxCapacity,
            const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
            unsigned int seed)
{
    std::ostringstream log;
    if (isValid(bag, maxCapacity, dependencyGraph)) {
        log << "\n[REPAIR] Bag is valid. Skip auto-repair.\n";
        std::cout << log.str();
        return true;
    }

    log << "\n[REPAIR] Bag invalid. Starting sequential auto-repair...\n";
    log << "Initial state: size=" << bag.getSize() << ", benefit=" << bag.getBenefit()
        << " (Capacity: " << maxCapacity << ")\n";

    Bag bagSmart = bag;
    Bag bagProb = bag;
    Bag bagTemp = bag;

    // Sequential repairs
    fixWithStrategy(bagSmart, maxCapacity, dependencyGraph, FEASIBILITY_STRATEGY::SMART, seed);
    fixWithStrategy(bagProb, maxCapacity, dependencyGraph, FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY, seed);
    fixWithStrategy(bagTemp, maxCapacity, dependencyGraph, FEASIBILITY_STRATEGY::TEMPERATURE_BIASED, seed);

    Bag* bestBag = &bagSmart;
    std::string bestStrategy = "SMART";
    if (bagProb.getBenefit() > bestBag->getBenefit()) {
        bestBag = &bagProb;
        bestStrategy = "PROBABILISTIC_GREEDY";
    }
    if (bagTemp.getBenefit() > bestBag->getBenefit()) {
        bestBag = &bagTemp;
        bestStrategy = "TEMPERATURE_BIASED";
    }

    bag = std::move(*bestBag);

    log << "Best strategy chosen: " << bestStrategy << "\n";
    log << "After repair: size=" << bag.getSize() << " / " << maxCapacity
        << ", benefit=" << bag.getBenefit() << "\n";

    bool isValidAfterRepair = isValid(bag, maxCapacity, dependencyGraph);
    if (!isValidAfterRepair) log << "[WARNING] Bag remains invalid after repair!\n";
    std::cout << log.str();

    return isValidAfterRepair;
}

} // namespace SOLUTION_REPAIR
