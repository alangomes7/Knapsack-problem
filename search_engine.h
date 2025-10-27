#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <chrono>

#include "algorithm.h"

// Forward declarations
class Bag;
class Package;
class Dependency;

namespace SEARCH_ENGINE {
    /**
     * @brief Defines the types of moves the local search can perform.
    */
    enum class MovementType {
        ADD,
        SWAP_REMOVE_1_ADD_1,
        SWAP_REMOVE_1_ADD_2,
        SWAP_REMOVE_2_ADD_1,
        EJECTION_CHAIN,
        NONE
    };

    std::string toString(MovementType movement);
}

/**
 * @brief Implements various local search and metaheuristic components.
 */
class SearchEngine {
public:
    explicit SearchEngine(unsigned int seed = 0);

    // --- Public Metaheuristic Methods ---
    void localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                     const SEARCH_ENGINE::MovementType& moveType,
                     ALGORITHM::LOCAL_SEARCH localSearchMethod,
                     const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                     int maxIterationsWithoutImprovement, int maxIterations, const std::chrono::time_point<std::chrono::steady_clock>& deadline);
    int getSeed() const;
    std::mt19937& getRandomGenerator();

private:
    // --- Core Private Logic ---
    bool applyMovement(const SEARCH_ENGINE::MovementType& move, Bag& currentBag, int bagSize,
        const std::vector<Package*>& packagesOutsideBag,
        ALGORITHM::LOCAL_SEARCH localSearchMethod,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        int maxIterations);

    void perturbation(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                      double strength);

    // --- Individual Move Operators ---
    bool tryAddPackage(Bag& currentBag, int bagSize,
                       const std::vector<Package*>& packagesOutsideBag,
                       const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    // 1-1 Swap Operators
    bool exploreSwap11NeighborhoodFirstImprovement(Bag&, int, const std::vector<Package*>&, const std::unordered_map<const Package*, std::vector<const Dependency*>>&);
    bool exploreSwap11NeighborhoodRandomImprovement(Bag&, int, const std::vector<Package*>&, const std::unordered_map<const Package*, std::vector<const Dependency*>>&, int);
    bool exploreSwap11NeighborhoodBestImprovement(Bag&, int, const std::vector<Package*>&, const std::unordered_map<const Package*, std::vector<const Dependency*>>&, int);

    // 1-2, 2-1, and Ejection Chain Operators (Best Improvement & First Improvement)
    bool exploreSwap12NeighborhoodBestImprovement(Bag&, int, const std::vector<Package*>&, const std::unordered_map<const Package*, std::vector<const Dependency*>>&, int);
    bool exploreSwap21NeighborhoodBestImprovement(Bag&, int, const std::vector<Package*>&, const std::unordered_map<const Package*, std::vector<const Dependency*>>&, int);
    bool exploreEjectionChainNeighborhoodFirstImprovement(Bag &currentBag, int bagSize, const std::vector<Package *> &packagesOutsideBag, const std::unordered_map<const Package *, std::vector<const Dependency *>> &dependencyGraph);
    bool exploreEjectionChainNeighborhoodBestImprovement(Bag &, int, const std::vector<Package *> &, const std::unordered_map<const Package *, std::vector<const Dependency *>> &, int);

    // --- Utility Function ---
    void buildOutsidePackages(const std::unordered_set<const Package*>& packagesInBag,
                              const std::vector<Package*>& allPackages,
                              std::vector<Package*>& packagesOutsideBag);
    
    std::mt19937 m_rng;
    int m_seed;
};

#endif // SEARCH_ENGINE_H