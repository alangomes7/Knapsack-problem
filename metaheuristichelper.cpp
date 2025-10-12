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
    int weight = pkg->getDependenciesSize();
    if (weight <= 0) return std::numeric_limits<double>::max();
    return static_cast<double>(pkg->getBenefit()) / weight;
}

double MetaheuristicHelper::computeRemovalScore(const Package* pkg, int dependents, double eff) const noexcept {
    // Low efficiency + few dependents + low benefit = good removal target
    double dependentsPenalty = dependents * 50.0;
    return eff + dependentsPenalty + pkg->getBenefit() * 0.1;
}

int MetaheuristicHelper::getStrategyCount() const noexcept {
    return 8;
}

/**
 * @brief Probabilistic greedy removal (stochastic variant of greedy).
 * Each package removal probability is proportional to its inefficiency.
 */
bool MetaheuristicHelper::probabilisticGreedyRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    auto& pkgs = bag.getPackages();
    if (pkgs.empty()) return false;

    while (bag.getSize() > maxCapacity) {
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
    }
    return true;
}

/**
 * @brief Temperature-based removal (simulated annealing inspired)
 */
bool MetaheuristicHelper::temperatureBiasedRemoval(
    Bag& bag, int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    double temperature = std::max(1.0, std::sqrt(bag.getSize() - maxCapacity));

    while (bag.getSize() > maxCapacity) {
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
            temperature *= 0.95; // cool down
        } else {
            break;
        }
    }

    return bag.getSize() <= maxCapacity;
}

bool MetaheuristicHelper::makeItFeasible(Bag& bag, int maxCapacity, 
                                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    if (bag.getSize() <= maxCapacity) {
        return true; // Already feasible
    }

    // Cycle through different strategies to explore solution space
    // This ensures we don't get stuck in local optima
    int strategyIndex = m_lastUsedStrategy % getStrategyCount();
    bool result = makeItFeasibleWithStrategy(bag, maxCapacity, dependencyGraph, strategyIndex);
    
    // Update for next call - cycle to next strategy
    m_lastUsedStrategy = (m_lastUsedStrategy + 1) % getStrategyCount();
    
    return result;
}

bool MetaheuristicHelper::makeItFeasibleAdvanced(Bag& bag, int maxCapacity,
                                                  const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                                                  RemovalStrategy strategy) {
    if (bag.getSize() <= maxCapacity) {
        return true;
    }

    switch (strategy) {
        case RemovalStrategy::SMART_REMOVAL:
            return smartRemoval(bag, maxCapacity, bag.getSize() - maxCapacity, dependencyGraph);
        
        case RemovalStrategy::GREEDY_REMOVAL:
            return greedyRemoval(bag, maxCapacity, dependencyGraph);
        
        case RemovalStrategy::MAXIMIZE_BENEFIT:
            return maximizeBenefitRemoval(bag, maxCapacity, dependencyGraph);
        
        case RemovalStrategy::MINIMIZE_LOSS:
            return minimizeLossRemoval(bag, maxCapacity, dependencyGraph);
        
        case RemovalStrategy::RANDOM_BIASED:
            return randomBiasedRemoval(bag, maxCapacity, dependencyGraph);
        
        case RemovalStrategy::BALANCED:
            return smartRemoval(bag, maxCapacity, bag.getSize() - maxCapacity, dependencyGraph) ||
                   greedyRemoval(bag, maxCapacity, dependencyGraph);
        
        default:
            return smartRemoval(bag, maxCapacity, bag.getSize() - maxCapacity, dependencyGraph) ||
                   greedyRemoval(bag, maxCapacity, dependencyGraph);
    }
}
bool MetaheuristicHelper::makeItFeasibleWithAllStrategies(Bag& bag, int maxCapacity,
                                                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    if (bag.getSize() <= maxCapacity) {
        return true;
    }

    // Try all strategies in sequence
    Bag originalBag = bag; // Keep a copy to restore if needed
    
    for (int i = 0; i < getStrategyCount(); ++i) {
        Bag testBag = originalBag; // Start fresh for each strategy
        if (makeItFeasibleWithStrategy(testBag, maxCapacity, dependencyGraph, i)) {
            bag = testBag; // Use the successful result
            return true;
        }
    }
    
    return false; // No strategy worked
}

bool MetaheuristicHelper::makeItFeasibleWithStrategy(Bag& bag, int maxCapacity,
                                                    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                                                    int strategyIndex) {
    if (bag.getSize() <= maxCapacity) {
        return true;
    }

    // Map index to strategy
    RemovalStrategy strategy;
    switch (strategyIndex % getStrategyCount()) {
        case 0: strategy = RemovalStrategy::SMART_REMOVAL; break;
        case 1: strategy = RemovalStrategy::GREEDY_REMOVAL; break;
        case 2: strategy = RemovalStrategy::MAXIMIZE_BENEFIT; break;
        case 3: strategy = RemovalStrategy::MINIMIZE_LOSS; break;
        case 4: strategy = RemovalStrategy::RANDOM_BIASED; break;
        case 5: strategy = RemovalStrategy::BALANCED; break;
        default: strategy = RemovalStrategy::BALANCED; break;
    }

    return makeItFeasibleAdvanced(bag, maxCapacity, dependencyGraph, strategy);
}

bool MetaheuristicHelper::smartRemoval(Bag& bag, int maxCapacity, int excessSize,
                                        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    const auto& packagesInBag = bag.getPackages();
    if (packagesInBag.empty()) return false;

    // Build removal candidates with comprehensive scoring
    struct RemovalCandidate {
        const Package* pkg;
        int directWeight;
        int benefit;
        double efficiency;          // benefit per weight
        double impactScore;         // composite score considering multiple factors
        int dependentsCount;        // how many packages depend on this
    };

    std::vector<RemovalCandidate> candidates;
    candidates.reserve(packagesInBag.size());

    // Build dependency impact map (which packages depend on each package)
    std::unordered_map<const Package*, std::unordered_set<const Package*>> dependentsMap;
    for (const Package* pkg : packagesInBag) {
        auto it = dependencyGraph.find(pkg);
        if (it != dependencyGraph.end()) {
            for (const Dependency* dep : it->second) {
                // Get all associated packages from this dependency
                const auto& assocPackages = dep->getAssociatedPackages();
                for (const auto& [pkgId, depPkg] : assocPackages) {
                    if (depPkg && packagesInBag.count(depPkg) > 0) {
                        // depPkg is a dependency of pkg, so pkg depends on depPkg
                        dependentsMap[depPkg].insert(pkg);
                    }
                }
            }
        }
    }

    // Score each package for removal
    for (const Package* pkg : packagesInBag) {
        RemovalCandidate candidate;
        candidate.pkg = pkg;
        candidate.directWeight = pkg->getDependenciesSize();
        candidate.benefit = pkg->getBenefit();
        
        // Calculate efficiency
        candidate.efficiency = (candidate.directWeight > 0) ?
            static_cast<double>(candidate.benefit) / candidate.directWeight :
            (candidate.benefit <= 0 ? -1.0 : std::numeric_limits<double>::max());

        // Count how many packages depend on this one
        candidate.dependentsCount = dependentsMap[pkg].size();

        // Calculate composite impact score (lower is better for removal)
        // Factors:
        // 1. Low efficiency packages are better to remove
        // 2. Packages with many dependents should be kept (cascading removals)
        // 3. Low benefit packages are better to remove
        // 4. Consider the "opportunity cost" of removal
        
        double efficiencyPenalty = candidate.efficiency;
        double dependentsPenalty = candidate.dependentsCount * 100.0; // High penalty for dependencies
        double benefitPenalty = candidate.benefit;
        
        // Composite score (lower = better candidate for removal)
        candidate.impactScore = efficiencyPenalty + dependentsPenalty + benefitPenalty * 0.1;
        
        candidates.push_back(candidate);
    }

    // Sort by impact score (ascending - lowest impact first = best to remove)
    std::sort(candidates.begin(), candidates.end(),
              [](const RemovalCandidate& a, const RemovalCandidate& b) {
                  return a.impactScore < b.impactScore;
              });

    // Remove packages starting with lowest impact
    int removedWeight = 0;
    std::vector<const Package*> toRemove;
    
    for (const auto& candidate : candidates) {
        if (removedWeight >= excessSize) break;
        
        toRemove.push_back(candidate.pkg);
        removedWeight += candidate.directWeight;
        
        // If this package has dependents, they might be removed too
        // Account for potential cascading removals
        if (candidate.dependentsCount > 0) {
            // This is a conservative approach - we're aware of cascade but don't over-remove
            break; // Re-evaluate after this removal
        }
    }

    // Execute removals
    for (const Package* pkg : toRemove) {
        auto it = dependencyGraph.find(pkg);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        bag.removePackage(*pkg, deps);
        
        if (bag.getSize() <= maxCapacity) {
            return true;
        }
    }

    // Check if we're feasible now
    return bag.getSize() <= maxCapacity;
}

bool MetaheuristicHelper::greedyRemoval(Bag& bag, int maxCapacity,
                                        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    // Improved greedy removal: consider multiple metrics
    while (bag.getSize() > maxCapacity) {
        const auto& packagesInBag = bag.getPackages();
        if (packagesInBag.empty()) return false;

        const Package* worstPackageToRemove = nullptr;
        double worstScore = std::numeric_limits<double>::max();

        for (const Package* package : packagesInBag) {
            int packageWeight = package->getDependenciesSize();
            int benefit = package->getBenefit();
            
            // Multi-criteria scoring
            double efficiencyRatio = (packageWeight > 0) ?
                static_cast<double>(benefit) / packageWeight :
                (benefit <= 0 ? -1.0 : std::numeric_limits<double>::max());

            // Additional factors:
            // 1. Raw benefit (prefer removing low benefit)
            // 2. Efficiency ratio (benefit per weight)
            // 3. Small size preference when nearly feasible
            
            int excess = bag.getSize() - maxCapacity;
            double sizeBonus = (packageWeight >= excess * 0.8 && packageWeight <= excess * 1.2) ? 
                              0.8 : 1.0; // Prefer packages close to excess size
            
            // Composite score (lower is worse, better to remove)
            double score = efficiencyRatio * sizeBonus;
            
            if (score < worstScore) {
                worstScore = score;
                worstPackageToRemove = package;
            }
        }

        if (worstPackageToRemove) {
            auto it = dependencyGraph.find(worstPackageToRemove);
            const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
            bag.removePackage(*worstPackageToRemove, deps);
        } else {
            return false;
        }
    }
    return true;
}

bool MetaheuristicHelper::maximizeBenefitRemoval(Bag& bag, int maxCapacity,
                                                  const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    // Strategy: Remove packages to maximize final benefit
    // Use knapsack-like thinking: keep high value-density items
    
    const auto& packagesInBag = bag.getPackages();
    std::vector<const Package*> pkgVector(packagesInBag.begin(), packagesInBag.end());
    
    // Sort by efficiency (benefit/weight) descending
    std::sort(pkgVector.begin(), pkgVector.end(),
              [](const Package* a, const Package* b) {
                  double effA = (a->getDependenciesSize() > 0) ?
                      static_cast<double>(a->getBenefit()) / a->getDependenciesSize() :
                      std::numeric_limits<double>::max();
                  double effB = (b->getDependenciesSize() > 0) ?
                      static_cast<double>(b->getBenefit()) / b->getDependenciesSize() :
                      std::numeric_limits<double>::max();
                  return effA > effB;
              });

    // Keep packages with highest efficiency until we fit
    Bag tempBag(bag.getBagAlgorithm(), "temp");
    int currentSize = 0;
    
    for (const Package* pkg : pkgVector) {
        auto it = dependencyGraph.find(pkg);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        
        // Check if we can add this package
        if (currentSize + pkg->getDependenciesSize() <= maxCapacity) {
            if (tempBag.canAddPackage(*pkg, maxCapacity, deps)) {
                tempBag.addPackage(*pkg, deps);
                currentSize = tempBag.getSize();
            }
        }
    }
    
    // Replace original bag with optimized version
    if (tempBag.getSize() <= maxCapacity) {
        bag = tempBag;
        return true;
    }
    
    return false;
}

bool MetaheuristicHelper::minimizeLossRemoval(Bag& bag, int maxCapacity,
                                              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    // Strategy: Remove minimum benefit packages first
    while (bag.getSize() > maxCapacity) {
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
        } else {
            return false;
        }
    }
    return true;
}

bool MetaheuristicHelper::randomBiasedRemoval(Bag& bag, int maxCapacity,
                                              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    // Strategy: Random removal biased towards low-efficiency packages
    while (bag.getSize() > maxCapacity) {
        const auto& packagesInBag = bag.getPackages();
        if (packagesInBag.empty()) return false;

        // Score packages (lower score = more likely to remove)
        std::vector<std::pair<const Package*, double>> scored;
        for (const Package* pkg : packagesInBag) {
            double eff = (pkg->getDependenciesSize() > 0) ?
                static_cast<double>(pkg->getBenefit()) / pkg->getDependenciesSize() : 10000.0;
            // Invert for removal probability (lower efficiency = higher removal chance)
            double removeProb = 1.0 / (eff + 1.0);
            scored.emplace_back(pkg, removeProb);
        }

        // Weighted random selection
        double totalProb = 0.0;
        for (const auto& [pkg, prob] : scored) {
            totalProb += prob;
        }

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
        } else {
            return false;
        }
    }
    return true;
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