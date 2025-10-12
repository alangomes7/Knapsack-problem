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
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph, int maxInterations)
{
    bool improvement_found = true;
    int iterations_without_improvement = 0;
    
    while (improvement_found && iterations_without_improvement < maxInterations) {
        switch (localSearchMethod) {
            case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT:
                improvement_found = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
            case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT:
                improvement_found = exploreSwapNeighborhoodRandomImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
            default:
                improvement_found = exploreSwapNeighborhoodFirstImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
        }
        
        if (!improvement_found) {
            iterations_without_improvement++;
        } else {
            iterations_without_improvement = 0;
        }
    }
    return improvement_found;
}

void LocalSearch::buildOutsidePackages(
    const std::unordered_set<const Package*>& packagesInBag,
    const std::vector<Package*>& allPackages,
    std::vector<Package*>& packagesOutsideBag)
{
    packagesOutsideBag.clear();
    packagesOutsideBag.reserve(allPackages.size() - packagesInBag.size());
    for (Package* p : allPackages) {
        if (packagesInBag.count(p) == 0) {
            packagesOutsideBag.emplace_back(p);
        }
    }
    
    // Sort by benefit descending for better pruning
    std::sort(packagesOutsideBag.begin(), packagesOutsideBag.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() > b->getBenefit();
              });
}

bool LocalSearch::exploreSwapNeighborhoodFirstImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    // Sort packages in bag by benefit ascending (try removing low-benefit first)
    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());
    std::sort(packagesInVec.begin(), packagesInVec.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() < b->getBenefit();
              });

    for (const Package* packageIn : packagesInVec) {
        const int benefitIn = packageIn->getBenefit();
        const auto& depsIn = dependencyGraph.at(packageIn);

        for (Package* packageOut : packagesOutsideBag) {
            const int benefitOut = packageOut->getBenefit();
            
            // Early termination: if current outside package has lower benefit than inside,
            // all subsequent ones will too (since sorted)
            if (benefitOut <= benefitIn) break;
            
            if (!currentBag.canSwap(*packageIn, *packageOut, bagSize)) continue;

            if (m_metaheuristicHelper.makeItFeasible(currentBag, bagSize, dependencyGraph)) {
                const auto& depsOut = dependencyGraph.at(packageOut);
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
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());
    const int nIn = (int)packagesInVec.size();
    const int nOut = (int)packagesOutsideBag.size();

    // Use thread_local for better performance
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> disIn(0, nIn - 1);
    std::uniform_int_distribution<size_t> disOut(0, nOut - 1);

    // Adaptive sampling: more tries for larger neighborhoods
    const int maxTries = std::min(300, std::max(50, static_cast<int>(std::sqrt(nIn * nOut) * 5)));

    // Track tried combinations to avoid redundant checks
    struct PairHash {
        std::size_t operator()(const std::pair<const Package*, const Package*>& p) const {
            return std::hash<const Package*>{}(p.first) ^ (std::hash<const Package*>{}(p.second) << 1);
        }
    };
    std::unordered_set<std::pair<const Package*, const Package*>, PairHash> triedSwaps;
    triedSwaps.reserve(maxTries);

    for (int i = 0; i < maxTries; ++i) {
        const Package* packageIn = packagesInVec[disIn(gen)];
        Package* packageOut = packagesOutsideBag[disOut(gen)];

        // Skip if already tried
        auto swapPair = std::make_pair(packageIn, packageOut);
        if (!triedSwaps.insert(swapPair).second) continue;

        if (packageOut->getBenefit() <= packageIn->getBenefit()) continue;
        if (!currentBag.canSwap(*packageIn, *packageOut, bagSize * 2)) continue;

        if (m_metaheuristicHelper.makeItFeasible(currentBag, bagSize, dependencyGraph)) {
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

    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());
    
    // Sort packages in bag by benefit ascending for better pruning
    std::sort(packagesInVec.begin(), packagesInVec.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() < b->getBenefit();
              });

    struct BestSwap {
        std::atomic<int> maxDelta{std::numeric_limits<int>::min()};
        std::atomic<const Package*> bestIn{nullptr};
        std::atomic<Package*> bestOut{nullptr};
    } sharedBest;

    // Parallel scan with improved pruning
    std::for_each(
        std::execution::par_unseq,
        packagesInVec.begin(), packagesInVec.end(),
        [&](const Package* packageIn) {
            const int benefitIn = packageIn->getBenefit();
            
            int localBestDelta = 0;
            const Package* localBestIn = nullptr;
            Package* localBestOut = nullptr;

            // Read current best to enable early termination
            int currentBest = sharedBest.maxDelta.load(std::memory_order_relaxed);

            for (Package* packageOut : packagesOutsideBag) {
                const int delta = packageOut->getBenefit() - benefitIn;
                
                // Pruning: if delta won't beat current best, skip
                if (delta <= currentBest) break;
                
                if (delta <= localBestDelta) continue;

                // Lightweight feasibility check
                if (!currentBag.canSwapReadOnly(*packageIn, *packageOut, bagSize * 1.5)) continue;

                localBestDelta = delta;
                localBestIn = packageIn;
                localBestOut = packageOut;
                
                // Update our view of the global best
                currentBest = std::max(currentBest, 
                    sharedBest.maxDelta.load(std::memory_order_relaxed));
            }

            // Atomically update shared best
            if (localBestDelta > 0) {
                int expected = sharedBest.maxDelta.load(std::memory_order_relaxed);
                while (localBestDelta > expected &&
                       !sharedBest.maxDelta.compare_exchange_weak(
                           expected, localBestDelta,
                           std::memory_order_release,
                           std::memory_order_relaxed)) {
                    // Loop continues with updated 'expected'
                }
                if (localBestDelta > expected) {
                    sharedBest.bestIn.store(localBestIn, std::memory_order_release);
                    sharedBest.bestOut.store(localBestOut, std::memory_order_release);
                }
            }
        });

    const Package* bestIn = sharedBest.bestIn.load(std::memory_order_acquire);
    Package* bestOut = sharedBest.bestOut.load(std::memory_order_acquire);

    if (bestIn && bestOut) {
        const auto& depsIn = dependencyGraph.at(bestIn);
        const auto& depsOut = dependencyGraph.at(bestOut);

        // Verify and apply swap
        if (currentBag.canSwap(*bestIn, *bestOut, bagSize) &&
            m_metaheuristicHelper.makeItFeasible(currentBag, bagSize, dependencyGraph)) {
            currentBag.removePackage(*bestIn, depsIn);
            currentBag.addPackage(*bestOut, depsOut);
            return true;
        }
    }

    return false;
}