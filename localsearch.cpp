#include "localsearch.h"

#include <random>
#include <algorithm>
#include <execution>
#include <atomic>
#include <tuple>
#include <limits>
#include <cmath>

#include "bag.h"
#include "package.h"
#include "dependency.h"

bool LocalSearch::run(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    Algorithm::LOCAL_SEARCH localSearchMethod,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    bool improvementFoundInIteration = true;
    int iterationsWithoutImprovement = 0;
    
    // Pre-allocate vector for packages outside the bag to avoid repeated allocations
    std::vector<Package*> packagesOutsideBag;
    packagesOutsideBag.reserve(allPackages.size());

    while (improvementFoundInIteration && iterationsWithoutImprovement < maxIterations) {
        improvementFoundInIteration = false;

        // Rebuild the list of packages outside the bag once per iteration
        buildOutsidePackages(currentBag.getPackages(), allPackages, packagesOutsideBag);

        // --- Phase 1: Try adding a package ---
        // This is a greedy move to quickly improve the solution.
        if (tryAddPackage(currentBag, bagSize, packagesOutsideBag, dependencyGraph)) {
            improvementFoundInIteration = true;
            iterationsWithoutImprovement = 0;
            continue; // Restart the loop to try adding again
        }

        // --- Phase 2: Try swapping packages (the core of the local search) ---
        bool swapFound = false;
        switch (localSearchMethod) {
            case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT:
                swapFound = exploreSwapNeighborhoodBestImprovement(
                    currentBag, bagSize, allPackages, dependencyGraph);
                break;
            case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT:
                swapFound = exploreSwapNeighborhoodRandomImprovement(
                    currentBag, bagSize, allPackages, dependencyGraph);
                break;
            default: // First Improvement
                swapFound = exploreSwapNeighborhoodFirstImprovement(
                    currentBag, bagSize, allPackages, dependencyGraph);
                break;
        }

        if (swapFound) {
            improvementFoundInIteration = true;
            iterationsWithoutImprovement = 0;
            continue; // Restart loop after a successful swap
        }

        // --- Phase 3: Try removing a package (last resort) ---
        // This move is intended to escape a local optimum by freeing up space.
        if (tryRemovePackage(currentBag, dependencyGraph)) {
            improvementFoundInIteration = true; // A move was made, even if benefit decreased
            iterationsWithoutImprovement = 0;
            continue;
        }

        // If no move was made in this iteration, increment the counter.
        if (!improvementFoundInIteration) {
            ++iterationsWithoutImprovement;
        }
    }
    // The loop now maintains feasibility, so no final "makeItFeasible" call is needed.
    return iterationsWithoutImprovement < maxIterations;
}

bool LocalSearch::tryAddPackage(
    Bag& currentBag, int bagSize,
    const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    if (packagesOutsideBag.empty()) return false;

    // Create a mutable copy to sort
    std::vector<Package*> candidates = packagesOutsideBag;

    // IMPROVEMENT: Sort by benefit/size ratio for a smarter greedy choice.
    // This prioritizes packages that offer more benefit for their size footprint.
    std::sort(candidates.begin(), candidates.end(),
              [](const Package* a, const Package* b) {
                  // Add 1 to size to avoid division by zero for packages with no dependencies
                  double ratio_a = static_cast<double>(a->getBenefit()) / (a->getDependenciesSize() + 1);
                  double ratio_b = static_cast<double>(b->getBenefit()) / (b->getDependenciesSize() + 1);
                  return ratio_a > ratio_b;
              });

    for (Package* p : candidates) {
        // FIX: Check for feasibility *before* adding the package.
        // The canAdd method should calculate the real cost of adding, including new dependencies.
        if (currentBag.canAddPackage(*p, bagSize, dependencyGraph.at(p))) {
            const auto& deps = dependencyGraph.at(p);
            currentBag.addPackage(*p, deps);
            return true; // Found and applied a feasible "add" move.
        }
    }
    return false; // No package could be added without violating capacity.
}

bool LocalSearch::tryRemovePackage(
    Bag& currentBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packages = currentBag.getPackages();
    if (packages.empty()) return false;
    
    const Package* worstPackage = nullptr;
    double worstRatio = std::numeric_limits<double>::max();
    
    // Find the package with the worst benefit/size ratio to remove.
    for (const Package* p : packages) {
        double ratio = static_cast<double>(p->getBenefit()) / (p->getDependenciesSize() + 1);
        if (ratio < worstRatio) {
            worstRatio = ratio;
            worstPackage = p;
        } 
    }
    
    if (worstPackage) {
        // FIX: Removed the check `if (currentBag.getBenefit() >= currentBenefit)`.
        // The goal of removal is to free up space to escape a local optimum.
        // A decrease in benefit is acceptable and expected.
        const auto& deps = dependencyGraph.at(worstPackage);
        currentBag.removePackage(*worstPackage, deps);
        return true;
    }
    
    return false;
}

void LocalSearch::buildOutsidePackages(
    const std::unordered_set<const Package*>& packagesInBag,
    const std::vector<Package*>& allPackages,
    std::vector<Package*>& packagesOutsideBag)
{
    packagesOutsideBag.clear();
    for (Package* p : allPackages) {
        if (packagesInBag.find(p) == packagesInBag.end()) {
            packagesOutsideBag.push_back(p);
        }
    }
}

// --- Swap Neighborhood Implementations ---
// All swap functions are updated to check feasibility before acting
// and to perform the swap directly without a repair step.

bool LocalSearch::exploreSwapNeighborhoodFirstImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    // Sort to prioritize trying better swaps first
    std::sort(packagesOutsideBag.begin(), packagesOutsideBag.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() > b->getBenefit();
              });

    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());
    std::sort(packagesInVec.begin(), packagesInVec.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() < b->getBenefit();
              });

    for (const Package* packageIn : packagesInVec) {
        for (Package* packageOut : packagesOutsideBag) {
            // Only consider swaps that improve the benefit
            if (packageOut->getBenefit() <= packageIn->getBenefit()) break;
            
            // FIX: Feasibility is checked by canSwap before the move is made.
            if (currentBag.canSwap(*packageIn, *packageOut, bagSize)) {
                const auto& depsIn = dependencyGraph.at(packageIn);
                const auto& depsOut = dependencyGraph.at(packageOut);
                
                // Perform the swap directly. The solution remains feasible.
                currentBag.removePackage(*packageIn, depsIn);
                currentBag.addPackage(*packageOut, depsOut);
                return true;
            }
        }
    }
    return false;
}

bool LocalSearch::exploreSwapNeighborhoodRandomImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.size() < 2) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());
    
    thread_local std::mt19937 gen(std::random_device{}());
    
    const int nIn = static_cast<int>(packagesInVec.size());
    const int nOut = static_cast<int>(packagesOutsideBag.size());
    const int maxTries = std::min(500, nIn * nOut);

    std::uniform_int_distribution<int> disIn(0, nIn - 1);
    std::uniform_int_distribution<int> disOut(0, nOut - 1);

    for (int i = 0; i < maxTries; ++i) {
        const Package* packageIn = packagesInVec[disIn(gen)];
        Package* packageOut = packagesOutsideBag[disOut(gen)];

        if (packageOut->getBenefit() <= packageIn->getBenefit()) continue;

        // FIX: Check feasibility first.
        if (currentBag.canSwap(*packageIn, *packageOut, bagSize)) {
            const auto& depsIn = dependencyGraph.at(packageIn);
            const auto& depsOut = dependencyGraph.at(packageOut);
            
            currentBag.removePackage(*packageIn, depsIn);
            currentBag.addPackage(*packageOut, depsOut);
            return true;
        }
    }
    return false;
}

bool LocalSearch::exploreSwapNeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    struct BestSwap {
        int delta = 0;
        const Package* packageIn = nullptr;
        Package* packageOut = nullptr;
    };

    // This struct will hold the best swap found by any thread.
    BestSwap bestOverallSwap;
    // Mutex to protect access to the bestOverallSwap struct
    std::mutex bestSwapMutex;

    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());

    // Parallel evaluation of all possible swaps
    std::for_each(std::execution::par, packagesInVec.begin(), packagesInVec.end(),
        [&](const Package* packageIn) {
            BestSwap localBestSwap; // Best swap found by this thread

            for (Package* packageOut : packagesOutsideBag) {
                int currentDelta = packageOut->getBenefit() - packageIn->getBenefit();

                // Pruning: if this swap isn't better than the best one found so far
                // by this thread, skip the expensive feasibility check.
                if (currentDelta <= localBestSwap.delta) continue;

                // FIX: Check for feasibility before considering it the best swap.
                if (currentBag.canSwap(*packageIn, *packageOut, bagSize)) {
                    localBestSwap = {currentDelta, packageIn, packageOut};
                }
            }

            // If this thread found a valid swap, try to update the global best.
            if (localBestSwap.packageIn) {
                std::lock_guard<std::mutex> lock(bestSwapMutex);
                if (localBestSwap.delta > bestOverallSwap.delta) {
                    bestOverallSwap = localBestSwap;
                }
            }
        });

    // After checking all pairs, perform the best valid swap found.
    if (bestOverallSwap.packageIn) {
        const auto& depsIn = dependencyGraph.at(bestOverallSwap.packageIn);
        const auto& depsOut = dependencyGraph.at(bestOverallSwap.packageOut);

        currentBag.removePackage(*bestOverallSwap.packageIn, depsIn);
        currentBag.addPackage(*bestOverallSwap.packageOut, depsOut);
        return true;
    }

    return false;
}