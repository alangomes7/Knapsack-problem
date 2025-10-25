#ifndef BAG_H
#define BAG_H

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "algorithm.h"
#include "search_engine.h"
#include "solution_repair.h"

class Package;
class Dependency;
class Algorithm;

class Bag {
public:
    // --- Constructors ---
    explicit Bag(ALGORITHM::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp);
    explicit Bag(const std::vector<Package*>& packages,
                 const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    // --- Getters ---
    const std::unordered_set<const Package*>& getPackages() const;
    const std::unordered_set<const Dependency*>& getDependencies() const;
    const std::unordered_map<const Dependency*, int>& getDependencyRefCount() const;

    int getSize() const;
    int getBenefit() const;
    ALGORITHM::ALGORITHM_TYPE getBagAlgorithm() const;
    ALGORITHM::LOCAL_SEARCH getBagLocalSearch() const;
    SEARCH_ENGINE::MovementType getMovementType() const;
    double getAlgorithmTime() const;
    std::string getAlgorithmTimeString() const;
    std::string getTimestamp() const;
    std::string getMetaheuristicParameters() const;
    SOLUTION_REPAIR::FEASIBILITY_STRATEGY getFeasibilityStrategy() const;

    // --- Setters ---
    void setTimestamp(const std::string& timestamp);
    void setAlgorithmTime(double seconds);
    void setLocalSearch(ALGORITHM::LOCAL_SEARCH localSearch);
    void setBagAlgorithm(ALGORITHM::ALGORITHM_TYPE bagAlgorithm);
    void setMovementType(SEARCH_ENGINE::MovementType movementType);
    void setMetaheuristicParameters(const std::string& params);
    void setFeasibilityStrategy(SOLUTION_REPAIR::FEASIBILITY_STRATEGY feasibilityStrategy);

    // --- Core Bag Operations ---
    bool addPackage(const Package& package, const std::vector<const Dependency*>& dependencies);
    void removePackage(const Package& package, const std::vector<const Dependency*>& dependencies);

    // --- Feasibility Checks ---
    bool canAddPackage(const Package& package, int maxCapacity, const std::vector<const Dependency*>& dependencies) const;
    
    // --- (DEPRECATED) Old canSwap method, kept for compatibility if needed ---
    bool canSwap(const Package& packageIn, const Package& packageOut, int bagSize) const;

    /**
     * @brief Checks if a 1-for-1 swap is feasible without modifying the bag.
     * This version correctly handles shared dependencies.
     */
    bool canSwapReadOnly(const Package& packageIn, const Package& packageOut, int bagSize) const noexcept;

    /**
     * @brief Checks if swapping one package in the bag for multiple packages outside is feasible.
     */
    bool canSwapReadOnly(const Package& packageIn, const std::vector<Package*>& packagesOut, int bagSize,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const noexcept;

    /**
     * @brief Checks if swapping multiple packages in the bag for one package outside is feasible.
     */
    bool canSwapReadOnly(const std::vector<const Package*>& packagesIn, const Package& packageOut, int bagSize,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const noexcept;
    
    /**
     * @brief Identifies packages in the bag whose dependencies are no longer met.
     * This is essential for validating the bag's state after removals, especially for the Ejection Chain move.
     * @param dependencyGraph The global dependency graph for lookups.
     * @return A vector of const Package pointers that are now invalid.
     */
    std::vector<const Package*> getInvalidPackages(
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const;

    // --- Utility ---
    std::string toString() const;

private:
    ALGORITHM::ALGORITHM_TYPE m_bagAlgorithm;
    ALGORITHM::LOCAL_SEARCH m_localSearch;
    SEARCH_ENGINE::MovementType m_movementType;
    SOLUTION_REPAIR::FEASIBILITY_STRATEGY m_feasibilityStrategy;
    std::string m_timeStamp = "0000-00-00 00:00:00";
    int m_size;
    int m_benefit;
    double m_algorithmTimeSeconds;
    std::string m_metaheuristicParams;

    std::unordered_set<const Package*> m_baggedPackages;
    std::unordered_set<const Dependency*> m_baggedDependencies;
    std::unordered_map<const Dependency*, int> m_dependencyRefCount;
};

#endif // BAG_H