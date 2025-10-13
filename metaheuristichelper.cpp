#include "MetaheuristicHelper.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <algorithm>
#include <limits>
#include <cmath>
#include <unordered_set>

MetaheuristicHelper::MetaheuristicHelper(unsigned int seed) : m_generator(seed) {}

double MetaheuristicHelper::computeEfficiency(const Package* pkg) const noexcept {
    double weight = pkg->getDependenciesSize();
    if (weight <= 0) return std::numeric_limits<double>::max();
    return static_cast<double>(pkg->getBenefit()) / weight;
}

double MetaheuristicHelper::computeRemovalScore(const Package* pkg, int dependents, double eff) const noexcept {
    // low efficiency + high dependents + low benefit = good removal target
    double dependentsPenalty = dependents;
    return eff + dependentsPenalty + (1 / pkg->getBenefit());
}

int MetaheuristicHelper::getStrategyCount() const noexcept {
    return 8;
}

bool MetaheuristicHelper::makeItFeasible(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (bag.getSize() <= maxCapacity)
        return true;

    int attemptCount = 0;
    const int maxAttempts = bag.getPackages().size();
     int strategyIndex = 0;
    while (bag.getSize() > maxCapacity && attemptCount < maxAttempts) {
        
        // Remove ONE package using the selected strategy
        bool removed = removeOnePackageWithStrategy(bag, maxCapacity, dependencyGraph, strategyIndex);
        
        if (!removed) {
            // If removal failed, try another strategy
            continue;
        }
        // Pick a random strategy
        strategyIndex = randomNumberInt(0, getStrategyCount() - 1);
        ++attemptCount;

        // Check if we're now feasible
        if (bag.getSize() <= maxCapacity)
            return true;

        // Switch to another random strategy for next iteration
        ++attemptCount;
    }

    // If still not feasible after all attempts
    return bag.getSize() <= maxCapacity;
}

bool MetaheuristicHelper::removeOnePackageWithStrategy(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int strategyIndex)
{
    if (bag.getPackages().empty()) return false;

    switch (strategyIndex % getStrategyCount()) {
        case 0: return smartRemoval(bag, maxCapacity, dependencyGraph);
        case 1: return greedyRemoval(bag, maxCapacity, dependencyGraph);
        case 2: return minimizeLossRemoval(bag, maxCapacity, dependencyGraph);
        case 3: return randomBiasedRemoval(bag, maxCapacity, dependencyGraph);
        case 4: return probabilisticGreedyRemoval(bag, maxCapacity, dependencyGraph);
        case 5: return temperatureBiasedRemoval(bag, maxCapacity, dependencyGraph);
        case 6: return maxWeightRemoval(bag, maxCapacity, dependencyGraph);
        case 7: return minEfficiencyRemoval(bag, maxCapacity, dependencyGraph);
        default: return smartRemoval(bag, maxCapacity, dependencyGraph);
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
                const auto& assocPackages = dep->getAssociatedPackages();
                for (const auto& [pkgId, depPkg] : assocPackages) {
                    if (depPkg && packagesInBag.count(depPkg) > 0) {
                        dependentsMap[depPkg].insert(pkg);
                    }
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

bool MetaheuristicHelper::greedyRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    const Package* worstPackage = nullptr;
    double worstScore = std::numeric_limits<double>::max();

    for (const Package* pkg : packagesInBag) {
        int weight = pkg->getDependenciesSize();
        int benefit = pkg->getBenefit();
        
        double efficiency = (weight > 0) ? static_cast<double>(benefit) / weight :
                           (benefit <= 0 ? -1.0 : std::numeric_limits<double>::max());
        
        if (efficiency < worstScore) {
            worstScore = efficiency;
            worstPackage = pkg;
        }
    }

    if (worstPackage) {
        auto it = dependencyGraph.find(worstPackage);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*worstPackage, deps);
        return true;
    }
    
    return false;
}

bool MetaheuristicHelper::minimizeLossRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    const Package* minBenefitPkg = nullptr;
    int minBenefit = std::numeric_limits<int>::max();

    for (const Package* pkg : packagesInBag) {
        if (pkg->getBenefit() < minBenefit) {
            minBenefit = pkg->getBenefit();
            minBenefitPkg = pkg;
        }
    }

    if (minBenefitPkg) {
        auto it = dependencyGraph.find(minBenefitPkg);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*minBenefitPkg, deps);
        return true;
    }
    
    return false;
}

bool MetaheuristicHelper::randomBiasedRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<std::pair<const Package*, double>> scored;
    for (const Package* pkg : packagesInBag) {
        double eff = (pkg->getDependenciesSize() > 0) ?
            static_cast<double>(pkg->getBenefit()) / pkg->getDependenciesSize() : 10000.0;
        double removeProb = 1.0 / (eff + 1.0);
        scored.emplace_back(pkg, removeProb);
    }

    double totalProb = 0.0;
    for (const auto& [pkg, prob] : scored) totalProb += prob;

    std::uniform_real_distribution<double> dist(0.0, totalProb);
    double randVal = dist(m_generator);
    
    const Package* selected = nullptr;
    double cumProb = 0.0;
    for (const auto& [pkg, prob] : scored) {
        cumProb += prob;
        if (randVal <= cumProb) {
            selected = pkg;
            break;
        }
    }

    if (!selected && !scored.empty()) {
        selected = scored.back().first;
    }

    if (selected) {
        auto it = dependencyGraph.find(selected);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*selected, deps);
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

    for (auto* pkg : pkgs) {
        double eff = computeEfficiency(pkg);
        scored.emplace_back(pkg, 1.0 / (eff + 1.0)); // lower efficiency = higher prob
    }

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

bool MetaheuristicHelper::maxWeightRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    const Package* maxWeightPkg = nullptr;
    int maxWeight = -1;

    for (const Package* pkg : packagesInBag) {
        int weight = pkg->getDependenciesSize();
        if (weight > maxWeight) {
            maxWeight = weight;
            maxWeightPkg = pkg;
        }
    }

    if (maxWeightPkg) {
        auto it = dependencyGraph.find(maxWeightPkg);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*maxWeightPkg, deps);
        return true;
    }
    
    return false;
}

bool MetaheuristicHelper::minEfficiencyRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    const Package* minEffPkg = nullptr;
    double minEff = std::numeric_limits<double>::max();

    for (const Package* pkg : packagesInBag) {
        double eff = computeEfficiency(pkg);
        if (eff < minEff) {
            minEff = eff;
            minEffPkg = pkg;
        }
    }

    if (minEffPkg) {
        auto it = dependencyGraph.find(minEffPkg);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*minEffPkg, deps);
        return true;
    }
    
    return false;
}

int MetaheuristicHelper::randomNumberInt(int min, int max)
{
    if (min > max) {
        return min;
    }
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(m_generator);
}

double MetaheuristicHelper::randomNumberDouble(double min, double max)
{
    if (min > max) {
        return min;
    }
    std::uniform_real_distribution<double> distribution(min, max);
    return distribution(m_generator);
}