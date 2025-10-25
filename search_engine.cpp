#include "search_engine.h"

#include <algorithm>
#include <vector>

#include "bag.h"
#include "package.h"
#include "dependency.h"

namespace SEARCH_ENGINE {
std::string toString(MovementType movement)
{
    switch (movement)
    {
        case MovementType::ADD:
            return "ADD";
        case MovementType::SWAP_REMOVE_1_ADD_1:
            return "SWAP_REMOVE_1_ADD_1";
        case MovementType::SWAP_REMOVE_1_ADD_2:
            return "SWAP_REMOVE_1_ADD_2";
        case MovementType::SWAP_REMOVE_2_ADD_1:
            return "SWAP_REMOVE_2_ADD_1";
        default:
        return "EJECTION_CHAIN";
    }
}
}

// =====================================================================================
// Public Methods
// =====================================================================================

SearchEngine::SearchEngine(unsigned int seed) : m_rng(seed), m_seed(seed) {}

// =====================================================================================
// Local Search (single movement type)
// =====================================================================================
void SearchEngine::localSearch(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const SEARCH_ENGINE::MovementType& moveType,
    ALGORITHM::LOCAL_SEARCH localSearchMethod,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterationsWithoutImprovement, int maxIterations, const std::chrono::time_point<std::chrono::steady_clock>& deadline)
{
    int iterationsWithoutImprovement = 0;
    currentBag.setLocalSearch(localSearchMethod);

    std::vector<Package*> sortedAll = allPackages;
    std::sort(sortedAll.begin(), sortedAll.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() > b->getBenefit();
              });

    std::vector<Package*> packagesOutsideBag;
    packagesOutsideBag.reserve(allPackages.size());

    while (iterationsWithoutImprovement < maxIterationsWithoutImprovement &&
           std::chrono::steady_clock::now() < deadline) {
        bool improvementFound = false;
        const int benefitBefore = currentBag.getBenefit();

        buildOutsidePackages(currentBag.getPackages(), sortedAll, packagesOutsideBag);

        if (applyMovement(moveType, currentBag, bagSize, packagesOutsideBag,
                          localSearchMethod, dependencyGraph, maxIterations))
        {
            if (currentBag.getBenefit() > benefitBefore) {
                improvementFound = true;
                iterationsWithoutImprovement = 0;
            }
        }

        if (!improvementFound)
            ++iterationsWithoutImprovement;
    }
}

int SearchEngine::getSeed() const
{
    return m_seed;
}

std::mt19937 & SearchEngine::getRandomGenerator()
{
    return m_rng;
}

// =====================================================================================
// Core Private Logic
// =====================================================================================

bool SearchEngine::applyMovement(const SEARCH_ENGINE::MovementType& move, Bag& currentBag, int bagSize,
    const std::vector<Package*>& packagesOutsideBag,
    ALGORITHM::LOCAL_SEARCH localSearchMethod,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    // Route the movement type to its corresponding neighborhood exploration function
    switch (move) {
        case SEARCH_ENGINE::MovementType::ADD:
            return tryAddPackage(currentBag, bagSize, packagesOutsideBag, dependencyGraph);
        
        case SEARCH_ENGINE::MovementType::SWAP_REMOVE_1_ADD_1:
            switch (localSearchMethod) {
                case ALGORITHM::LOCAL_SEARCH::BEST_IMPROVEMENT:
                    return exploreSwap11NeighborhoodBestImprovement(currentBag, bagSize, packagesOutsideBag, dependencyGraph, maxIterations);
                case ALGORITHM::LOCAL_SEARCH::RANDOM_IMPROVEMENT:
                    return exploreSwap11NeighborhoodRandomImprovement(currentBag, bagSize, packagesOutsideBag, dependencyGraph, maxIterations);
                default: // FIRST_IMPROVEMENT
                    return exploreSwap11NeighborhoodFirstImprovement(currentBag, bagSize, packagesOutsideBag, dependencyGraph);
            }
        case SEARCH_ENGINE::MovementType::SWAP_REMOVE_1_ADD_2:
            return exploreSwap12NeighborhoodBestImprovement(currentBag, bagSize, packagesOutsideBag, dependencyGraph, maxIterations);

        case SEARCH_ENGINE::MovementType::SWAP_REMOVE_2_ADD_1:
            return exploreSwap21NeighborhoodBestImprovement(currentBag, bagSize, packagesOutsideBag, dependencyGraph, maxIterations);
        
        case SEARCH_ENGINE::MovementType::EJECTION_CHAIN:
            return exploreEjectionChainNeighborhoodBestImprovement(currentBag, bagSize, packagesOutsideBag, dependencyGraph, maxIterations);
    }
    return false;
}

void SearchEngine::perturbation(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    double strength)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return;

    std::vector<const Package*> packagesVec(packagesInBag.begin(), packagesInBag.end());
    std::shuffle(packagesVec.begin(), packagesVec.end(), m_rng);

    // --- Step 1: Remove a percentage of packages
    const int removeCount = std::max(1, static_cast<int>(packagesVec.size() * strength));
    for (int i = 0; i < removeCount && i < packagesVec.size(); ++i) {
        currentBag.removePackage(*packagesVec[i], dependencyGraph.at(packagesVec[i]));
    }

    // --- Step 2: Greedily add a few feasible random packages
    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(currentBag.getPackages(), allPackages, packagesOutsideBag);
    std::shuffle(packagesOutsideBag.begin(), packagesOutsideBag.end(), m_rng);

    int addCount = 0;
    for (Package* candidate : packagesOutsideBag) {
        if (addCount >= removeCount) break;
        if (currentBag.addPackageIfPossible(*candidate, bagSize, dependencyGraph.at(candidate))) {
            addCount++;
        }
    }
}

// =====================================================================================
// Individual Move Operators
// =====================================================================================

/**
 * @brief Greedily adds the highest-benefit package that fits.
 */
bool SearchEngine::tryAddPackage(
    Bag& currentBag, int bagSize,
    const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    for (Package* p : packagesOutsideBag) {
        if (currentBag.addPackageIfPossible(*p, bagSize, dependencyGraph.at(p))) {
            return true;
        }
    }
    return false;
}

// --- 1-1 Swap Operators ---

bool SearchEngine::exploreSwap11NeighborhoodFirstImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    std::vector<const Package*> packagesInVec(currentBag.getPackages().begin(), currentBag.getPackages().end());
    if (packagesInVec.empty() || packagesOutsideBag.empty()) return false;
    
    for (const Package* packageIn : packagesInVec) {
        for (Package* packageOut : packagesOutsideBag) {
            if (packageOut->getBenefit() <= packageIn->getBenefit()) continue;
            // <-- FIX: Pass vectors {packageIn} and {packageOut} and the dependencyGraph
            if (currentBag.canSwapReadOnly({packageIn}, {packageOut}, bagSize, dependencyGraph)) {
                currentBag.removePackage(*packageIn, dependencyGraph.at(packageIn));
                // <-- FIX: Call addPackageIfPossible and pass bagSize
                currentBag.addPackageIfPossible(*packageOut, bagSize, dependencyGraph.at(packageOut));
                return true;
            }
        }
    }
    return false;
}

bool SearchEngine::exploreSwap11NeighborhoodRandomImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.size() < 2 || packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInVec(packagesInBag.begin(), packagesInBag.end());
    std::uniform_int_distribution<int> disIn(0, (int)packagesInVec.size() - 1);
    std::uniform_int_distribution<int> disOut(0, (int)packagesOutsideBag.size() - 1);

    for (int i = 0; i < maxIterations; ++i) {
        const Package* packageIn = packagesInVec[disIn(m_rng)];
        Package* packageOut = packagesOutsideBag[disOut(m_rng)];

        if (packageOut->getBenefit() <= packageIn->getBenefit()) continue;
        // <-- FIX: Pass vectors {packageIn} and {packageOut} and the dependencyGraph
        if (currentBag.canSwapReadOnly({packageIn}, {packageOut}, bagSize, dependencyGraph)) {
            currentBag.removePackage(*packageIn, dependencyGraph.at(packageIn));
            // <-- FIX: Call addPackageIfPossible and pass bagSize
            currentBag.addPackageIfPossible(*packageOut, bagSize, dependencyGraph.at(packageOut));
            return true;
        }
    }
    return false;
}

bool SearchEngine::exploreSwap11NeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    // Create sorted copies to avoid modifying original collections
    std::vector<const Package*> sortedPackagesIn(currentBag.getPackages().begin(), currentBag.getPackages().end());
    std::vector<Package*> sortedPackagesOut = packagesOutsideBag;

    if (sortedPackagesIn.empty() || sortedPackagesOut.empty()) return false;

    // --- OPTIMIZATION START ---
    // 1. Sort packages IN the bag by ASCENDING benefit (worst to best)
    std::sort(sortedPackagesIn.begin(), sortedPackagesIn.end(), 
              [](const Package* a, const Package* b) {
                  return a->getBenefit() < b->getBenefit();
              });

    // 2. Sort packages OUTSIDE the bag by DESCENDING benefit (best to worst)
    std::sort(sortedPackagesOut.begin(), sortedPackagesOut.end(),
              [](const Package* a, const Package* b) {
                  return a->getBenefit() > b->getBenefit();
              });
    // --- OPTIMIZATION END ---

    struct BestSwap { int delta = 0; const Package* p_in = nullptr; Package* p_out = nullptr; };
    BestSwap bestSwap;

    int iterations = 0;
    for (const Package* p_in : sortedPackagesIn) {
        for (Package* p_out : sortedPackagesOut) {
            if (++iterations > maxIterations) break;

            int potential_delta = p_out->getBenefit() - p_in->getBenefit();

            // --- PRUNING LOGIC ---
            // If the potential improvement is already less than or equal to the best
            // improvement found so far, no subsequent 'p_out' for this 'p_in'
            // will be better, because sortedPackagesOut is in descending order.
            // We can break the inner loop and move to the next p_in.
            if (potential_delta <= bestSwap.delta) {
                break; 
            }
            // --- END PRUNING LOGIC ---
            
            // <-- FIX: Pass vectors {p_in} and {p_out} and the dependencyGraph
            if (currentBag.canSwapReadOnly({p_in}, {p_out}, bagSize, dependencyGraph)) {
                bestSwap = {potential_delta, p_in, p_out};
            }
        }
        if (iterations > maxIterations) break;
    }

    if (bestSwap.p_in) {
        currentBag.removePackage(*bestSwap.p_in, dependencyGraph.at(bestSwap.p_in));
        // <-- FIX: Call addPackageIfPossible and pass bagSize
        currentBag.addPackageIfPossible(*bestSwap.p_out, bagSize, dependencyGraph.at(bestSwap.p_out));
        return true;
    }
    return false;
}

// --- 1-2, 2-1, and Ejection Chain Operators ---

/**
 * @brief Finds the best swap of 1 package in the bag for 2 packages outside.
 * Useful for filling small remaining capacity gaps.
 */
bool SearchEngine::exploreSwap12NeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    std::vector<const Package*> packagesInVec(currentBag.getPackages().begin(), currentBag.getPackages().end());
    if (packagesInVec.empty() || packagesOutsideBag.size() < 2) return false;

    struct BestMove { int delta = 0; const Package* p_in = nullptr; Package* p_out1 = nullptr; Package* p_out2 = nullptr; };
    BestMove bestMove;

    int iterations = 0;
    for (const Package* p_in : packagesInVec) {
        for (size_t i = 0; i < packagesOutsideBag.size(); ++i) {
            for (size_t j = i + 1; j < packagesOutsideBag.size(); ++j) {
                if (++iterations > maxIterations) break;
                Package* p_out1 = packagesOutsideBag[i];
                Package* p_out2 = packagesOutsideBag[j];
                
                int delta = (p_out1->getBenefit() + p_out2->getBenefit()) - p_in->getBenefit();
                if (delta <= bestMove.delta) continue;
                
                // <-- FIX: Pass vector {p_in} as the first argument
                if (currentBag.canSwapReadOnly({p_in}, {p_out1, p_out2}, bagSize, dependencyGraph)) {
                    bestMove = {delta, p_in, p_out1, p_out2};
                }
            }
            if (iterations > maxIterations) break;
        }
        if (iterations > maxIterations) break;
    }

    if (bestMove.p_in) {
        currentBag.removePackage(*bestMove.p_in, dependencyGraph.at(bestMove.p_in));
        // <-- FIX: Call addPackageIfPossible and pass bagSize
        currentBag.addPackageIfPossible(*bestMove.p_out1, bagSize, dependencyGraph.at(bestMove.p_out1));
        // <-- FIX: Call addPackageIfPossible and pass bagSize
        currentBag.addPackageIfPossible(*bestMove.p_out2, bagSize, dependencyGraph.at(bestMove.p_out2));
        return true;
    }
    return false;
}

/**
 * @brief Finds the best swap of 2 packages in the bag for 1 package outside.
 * Useful for making larger, structural changes to the solution.
 */
bool SearchEngine::exploreSwap21NeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    std::vector<const Package*> packagesInVec(currentBag.getPackages().begin(), currentBag.getPackages().end());
    if (packagesInVec.size() < 2 || packagesOutsideBag.empty()) return false;

    struct BestMove { int delta = 0; const Package* p_in1 = nullptr; const Package* p_in2 = nullptr; Package* p_out = nullptr; };
    BestMove bestMove;

    int iterations = 0;
    for (size_t i = 0; i < packagesInVec.size(); ++i) {
        for (size_t j = i + 1; j < packagesInVec.size(); ++j) {
            const Package* p_in1 = packagesInVec[i];
            const Package* p_in2 = packagesInVec[j];

            for (Package* p_out : packagesOutsideBag) {
                if (++iterations > maxIterations) break;
                int delta = p_out->getBenefit() - (p_in1->getBenefit() + p_in2->getBenefit());
                if (delta <= bestMove.delta) continue;
                
                // <-- FIX: Pass vector {p_out} as the second argument
                if (currentBag.canSwapReadOnly({p_in1, p_in2}, {p_out}, bagSize, dependencyGraph)) {
                    bestMove = {delta, p_in1, p_in2, p_out};
                }
            }
             if (iterations > maxIterations) break;
        }
        if (iterations > maxIterations) break;
    }

    if (bestMove.p_in1) {
        currentBag.removePackage(*bestMove.p_in1, dependencyGraph.at(bestMove.p_in1));
        currentBag.removePackage(*bestMove.p_in2, dependencyGraph.at(bestMove.p_in2));
        // <-- FIX: Call addPackageIfPossible and pass bagSize
        currentBag.addPackageIfPossible(*bestMove.p_out, bagSize, dependencyGraph.at(bestMove.p_out));
        return true;
    }
    return false;
}

/**
 * @brief Finds a package to remove that also invalidates its dependencies (an "ejection chain"),
 * then finds the best single package to add into the newly freed space.
 */
bool SearchEngine::exploreEjectionChainNeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& packagesOutsideBag,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxIterations)
{
    // Initial checks for empty collections
    if (currentBag.getPackages().empty() || packagesOutsideBag.empty()) {
        return false;
    }

    struct BestMove {
        int delta = 0;
        std::vector<const Package*> ejectionSet;
        Package* packageToAdd = nullptr;
    };
    BestMove bestMove;

    // We need the original reference counts to revert to for each trigger package
    const auto& originalRefCount = currentBag.getDependencyRefCount();
    std::vector<const Package*> packagesInVec(currentBag.getPackages().begin(), currentBag.getPackages().end());
    int iterations = 0;

    // 1. Iterate through each package in the bag as a potential "trigger" for a chain reaction
    for (const Package* triggerPackage : packagesInVec) {
        if (++iterations > maxIterations) break;

        // --- Simulation Setup ---
        std::unordered_map<const Dependency*, int> tempRefCount = originalRefCount;
        std::vector<const Package*> currentEjectionSet;
        std::unordered_set<const Package*> processedForEjection; // Prevents reprocessing
        std::vector<const Package*> packagesToProcess = {triggerPackage}; // Start the cascade
        processedForEjection.insert(triggerPackage);

        // 2. Simulate the cascading removal of packages
        while (!packagesToProcess.empty()) {
            const Package* packageToRemove = packagesToProcess.back();
            packagesToProcess.pop_back();
            currentEjectionSet.push_back(packageToRemove);

            // Decrement dependency counts for the removed package
            for (const auto* dep : dependencyGraph.at(packageToRemove)) {
                if (tempRefCount.count(dep)) {
                    tempRefCount[dep]--;
                }
            }

            // Check if this removal invalidates any other packages still in the bag
            for (const Package* otherPackage : packagesInVec) {
                if (processedForEjection.count(otherPackage)) continue; // Already slated for removal

                // Check dependencies of the other package
                for (const auto* dep : dependencyGraph.at(otherPackage)) {
                    // If a required dependency now has zero references, this package is also invalid
                    if (tempRefCount.count(dep) && tempRefCount.at(dep) == 0) {
                        packagesToProcess.push_back(otherPackage);
                        processedForEjection.insert(otherPackage);
                        break; // Move to the next package
                    }
                }
            }
        }

        // If removing the trigger didn't invalidate anything else, it's not a chain. Skip it.
        if (currentEjectionSet.size() <= 1) {
            continue;
        }

        // --- Calculate Post-Removal State ---
        int removedBenefit = 0;
        int sizeAfterRemoval = currentBag.getSize();

        // Calculate benefit removed and size reduction
        for (const auto* p_removed : currentEjectionSet) {
            removedBenefit += p_removed->getBenefit();
        }
        for (const auto& pair : tempRefCount) {
            if (pair.second == 0 && originalRefCount.at(pair.first) > 0) {
                sizeAfterRemoval -= pair.first->getSize();
            }
        }

        // 3. Find the best package to add into the newly created space
        for (Package* p_out : packagesOutsideBag) {
            int delta = p_out->getBenefit() - removedBenefit;
            if (delta <= bestMove.delta) continue;

            // Check feasibility by calculating size increase
            int sizeIncrease = 0;
            for (const auto* dep : dependencyGraph.at(p_out)) {
                // If the dependency is not present in our simulated state, its size is added
                if (tempRefCount.count(dep) == 0 || tempRefCount.at(dep) == 0) {
                    sizeIncrease += dep->getSize();
                }
            }

            if ((sizeAfterRemoval + sizeIncrease) <= bagSize) {
                bestMove = {delta, currentEjectionSet, p_out};
            }
        }
    }

    // 4. If a valid, improving move was found, apply it to the actual bag
    if (bestMove.packageToAdd) {
        for (const auto* p_to_remove : bestMove.ejectionSet) {
            currentBag.removePackage(*p_to_remove, dependencyGraph.at(p_to_remove));
        }
        // <-- FIX: Call addPackageIfPossible and pass bagSize
        currentBag.addPackageIfPossible(*bestMove.packageToAdd, bagSize, dependencyGraph.at(bestMove.packageToAdd));
        return true;
    }

    return false;
}

// =====================================================================================
// Utility Functions
// =====================================================================================

void SearchEngine::buildOutsidePackages(
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