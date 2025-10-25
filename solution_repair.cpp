#include "solution_repair.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
// random_provider.h is no longer needed for randomness in this file
// #include "random_provider.h" 

#include <algorithm>
#include <unordered_set>
#include <limits>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>       // <-- ADDED: For std::thread
#include <mutex>        // <-- ADDED: For std::mutex (to protect cout)
#include <functional>   // <-- ADDED: For std::ref and std::cref
// <random> is included via solution_repair.h

namespace SOLUTION_REPAIR {

// --- Thread-Safe Additions ---
static std::mutex coutMutex; // Mutex for protecting std::cout

// --- Helper Struct for Scoring ---
struct PackageScore {
    const Package* pkg = nullptr;
    int benefit = 0;
    int uniqueSize = 0;
    double efficiency = 0.0;        // (benefit / uniqueSize) - higher is better
    double inefficiency = 0.0;      // (uniqueSize / benefit) - higher is worse
    double smartScore = 0.0;        // Composite score - lower is worse (target for removal)
};

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

/**
 * @brief Helper to compute the true size of the bag from scratch.
 */
static int computeBagSize(
    const Bag& bag,
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

/**
 * @brief (Internal) Validates the bag's internal state against reality.
 */
static bool isValid(
    const Bag& bag,
    int maxCapacity,
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

    // Check internal counters
    if (benefit != bag.getBenefit()) return false;
    if (size != bag.getSize()) return false;
    // Check capacity
    if (size > maxCapacity) return false;
    return true;
}

/**
 * @brief Calculates scores for all packages currently in the bag.
 */
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

        // Calculate the "unique size" this package contributes.
        // This is the size of dependencies that *only* this package uses.
        const auto& deps = dependencyGraph.at(pkg);
        for (const auto* dep : deps) {
            if (refCounts.at(dep) == 1) {
                s.uniqueSize += dep->getSize();
            }
        }

        // --- Calculate Scores ---
        // Efficiency (Benefit / Size) - higher is better
        if (s.uniqueSize > 0)
            s.efficiency = static_cast<double>(s.benefit) / s.uniqueSize;
        else
            s.efficiency = (s.benefit > 0) ? std::numeric_limits<double>::max() : 0.0; // High benefit/zero size is great

        // Inefficiency (Size / Benefit) - higher is worse
        if (s.benefit > 0)
            s.inefficiency = static_cast<double>(s.uniqueSize) / s.benefit;
        else
            s.inefficiency = (s.uniqueSize > 0) ? std::numeric_limits<double>::max() : 0.0; // High size/zero benefit is terrible

        // SMART Score: Combo of efficiency and benefit. Lower is worse.
        // Weights: 70% efficiency, 30% absolute benefit
        s.smartScore = (s.efficiency * 0.7) + (static_cast<double>(s.benefit) * 0.3);

        scores.push_back(s);
    }
    return scores;
}

/**
 * @brief Selects a package to remove using Roulette Wheel (Probabilistic Greedy).
 * Selects based on inefficiency (higher inefficiency = higher chance).
 */
// <-- MODIFIED: Takes a generator
static const Package* selectProbabilistic(const std::vector<PackageScore>& scores, std::mt19937& rng) {
    double totalInefficiency = 0.0;
    for (const auto& s : scores) {
        totalInefficiency += s.inefficiency;
    }

    if (totalInefficiency == 0.0) {
        // All packages are equally (in)efficient, pick one at random
        std::uniform_int_distribution<int> dist_idx(0, scores.size() - 1);
        int idx = dist_idx(rng); // <-- MODIFIED: Use passed rng
        return scores[idx].pkg;
    }

    std::uniform_real_distribution<double> dist_roll(0.0, totalInefficiency);
    double roll = dist_roll(rng); // <-- MODIFIED: Use passed rng
    double currentSum = 0.0;
    for (const auto& s : scores) {
        currentSum += s.inefficiency;
        if (roll <= currentSum) {
            return s.pkg;
        }
    }
    return scores.back().pkg; // Fallback
}

/**
 * @brief Selects a package to remove using Temperature Biased logic.
 * Adds random noise to scores, influenced by temperature.
 */
// <-- MODIFIED: Takes a generator
static const Package* selectTemperatureBiased(const std::vector<PackageScore>& scores, double temperature, std::mt19937& rng) {
    // Temperature is a fraction (e.g., 0.2 = 20% oversized)
    // Add random noise to the 'smartScore'
    
    const Package* worstPkg = nullptr;
    double worstScore = std::numeric_limits<double>::max();

    for (const auto& s : scores) {
        // Add random "noise" based on temperature.
        // Noise is a multiplier from (1.0 - temp) to (1.0 + temp)
        // e.g., temp=0.2 -> noise is 0.8 to 1.2
        std::uniform_real_distribution<double> dist_noise(1.0 - temperature, 1.0 + temperature);
        double noise = dist_noise(rng); // <-- MODIFIED: Use passed rng
        double noisyScore = s.smartScore * noise;

        if (noisyScore < worstScore) {
            worstScore = noisyScore;
            worstPkg = s.pkg;
        }
    }
    return worstPkg;
}


/**
 * @brief (Internal) The core repair heuristic.
 * This function modifies the bag in place until it is feasible.
 */
// <-- MODIFIED: Takes a seed
static bool fixWithStrategy(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    FEASIBILITY_STRATEGY strategy,
    unsigned int seed) // <-- ADDED: Seed for this thread
{
    // <-- ADDED: Create a local generator for this thread
    std::mt19937 thread_rng(seed); 

    int currentSize = computeBagSize(bag, dependencyGraph);
    double initialOver = static_cast<double>(currentSize - maxCapacity);

    while (currentSize > maxCapacity) {
        if (bag.getPackages().empty()) {
            break; // Should not happen, but safeguard
        }

        // 1. Score all packages in the current bag
        auto scores = scorePackages(bag, dependencyGraph);
        if (scores.empty()) break;

        // 2. Select package to remove based on strategy
        const Package* pkgToRemove = nullptr;

        switch (strategy) {
            case FEASIBILITY_STRATEGY::SMART: {
                // Remove the package with the LOWEST (worst) smartScore
                auto it = std::min_element(scores.begin(), scores.end(),
                    [](const PackageScore& a, const PackageScore& b) {
                        return a.smartScore < b.smartScore;
                    });
                pkgToRemove = it->pkg;
                break;
            }

            case FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY: {
                // Remove package via roulette wheel (higher ineffiency = higher chance)
                pkgToRemove = selectProbabilistic(scores, thread_rng); // <-- MODIFIED: Pass generator
                break;
            }

            case FEASIBILITY_STRATEGY::TEMPERATURE_BIASED: {
                // Remove package based on noisy score
                // Temperature is how oversized the bag is (0.0 to 1.0+)
                double temperature = std::max(0.0, (currentSize - maxCapacity) / initialOver);
                pkgToRemove = selectTemperatureBiased(scores, temperature, thread_rng); // <-- MODIFIED: Pass generator
                break;
            }
        }

        // 3. Remove the selected package
        if (pkgToRemove) {
            bag.removePackage(*pkgToRemove, dependencyGraph.at(pkgToRemove));
        }

        // 4. Re-compute size
        currentSize = computeBagSize(bag, dependencyGraph);
    }

    return (currentSize <= maxCapacity);
}

// =====================================================================================
// PUBLIC REPAIR FUNCTION (Single Entry Point)
// =====================================================================================

// <-- MODIFIED: Takes main generator
bool repair(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    std::mt19937& main_rng)
{
    // --- 0. LOG ---
    std::ostringstream log;
    // --- 1. VALIDATE ---
    if (isValid(bag, maxCapacity, dependencyGraph)) {
        log << "\n[REPAIR] Bag is valid. Skip auto-repair.\n";
        {
            std::lock_guard<std::mutex> lock(coutMutex);
            std::cout << log.str();
        }
        return true; // Bag is already valid, no action needed.
    }

    // --- 2. LOG & PREPARE FOR FIX ---
    log << "\n[REPAIR] Bag invalid. Starting auto-repair...\n";
    log << "Initial state: size=" << bag.getSize() << ", benefit=" << bag.getBenefit()
        << " (Capacity: " << maxCapacity << ")\n";
    log << "Testing 3 strategies in parallel: SMART, PROBABILISTIC_GREEDY, TEMPERATURE_BIASED\n";

    // --- 3. CREATE COPIES AND RUN STRATEGIES IN PARALLEL ---
    Bag bagSmart = bag;
    Bag bagProb = bag;
    Bag bagTemp = bag;

    // <-- MODIFIED: Generate unique, reproducible seeds from the main generator
    unsigned int seedSmart = main_rng();
    unsigned int seedProb = main_rng();
    unsigned int seedTemp = main_rng();

    // Launch threads, passing each its own unique seed
    std::thread smartThread(fixWithStrategy, std::ref(bagSmart), maxCapacity, std::cref(dependencyGraph), FEASIBILITY_STRATEGY::SMART, seedSmart);
    std::thread probThread(fixWithStrategy, std::ref(bagProb), maxCapacity, std::cref(dependencyGraph), FEASIBILITY_STRATEGY::PROBABILISTIC_GREEDY, seedProb);
    std::thread tempThread(fixWithStrategy, std::ref(bagTemp), maxCapacity, std::cref(dependencyGraph), FEASIBILITY_STRATEGY::TEMPERATURE_BIASED, seedTemp);

    // Wait for all three repair strategies to complete
    smartThread.join();
    probThread.join();
    tempThread.join();
    // <-- END MODIFICATION

    // --- 4. FIND THE BEST RESULT ---
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

    // --- 5. MOVE BEST BAG INTO ORIGINAL & LOG ---
    bag = std::move(*bestBag);

    log << "Best strategy chosen: " << bestStrategy << "\n";
    log << "After repair: size=" << bag.getSize()
        << " / " << maxCapacity
        << ", benefit=" << bag.getBenefit() << "\n";

    // --- 6. FINAL VALIDATION ---
    bool isValidAfterRepair = isValid(bag, maxCapacity, dependencyGraph);

    if (!isValidAfterRepair) {
        log << "[WARNING] Bag remains invalid after repair!\n";
    }
    
    {
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << log.str();
    }

    return isValidAfterRepair;
}

} // namespace SOLUTION_REPAIR