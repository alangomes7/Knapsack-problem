#ifndef METAHEURISTICHELPER_H
#define METAHEURISTICHELPER_H

#include <vector>
#include <unordered_map>
#include <random>

class Bag;
class Package;
class Dependency;

/**
 * @brief Utility class offering advanced helper methods for metaheuristics solving
 *        the knapsack problem with dependencies.
 *
 * It provides multiple strategies to restore feasibility by removing one package
 * at a time and switching strategies randomly until the bag becomes feasible.
 */
class MetaheuristicHelper {
public:
    explicit MetaheuristicHelper(unsigned int seed = std::random_device{}());

    /**
     * @brief Makes the bag feasible by iteratively removing one package at a time
     *        using randomly selected strategies until capacity constraint is met.
     * 
     * @param bag The bag to make feasible
     * @param maxCapacity Maximum allowed capacity
     * @param dependencyGraph Dependency relationships between packages
     * @return true if bag is feasible, false otherwise
     */
    bool makeItFeasible(Bag& bag, int maxCapacity,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    /**
     * @brief Returns the number of available removal strategies
     */
    int getStrategyCount() const noexcept;

    /**
     * @brief Generates a random integer in the range [min, max]
     */
    int randomNumberInt(int min, int max);

    /**
     * @brief Generates a random double in the range [min, max]
     */
    double randomNumberDouble(double min, double max);

private:
    /**
     * @brief Removes one package using the specified strategy
     * 
     * @param strategyIndex Index of the strategy to use (0 to getStrategyCount()-1)
     * @return true if a package was successfully removed, false otherwise
     */
    bool removeOnePackageWithStrategy(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&, int strategyIndex);

    // Single package removal strategies
    
    /**
     * @brief Smart removal: considers efficiency, benefit, and dependency impact
     */
    bool smartRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Greedy removal: removes package with worst benefit/weight ratio
     */
    bool greedyRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Minimize loss: removes package with lowest benefit
     */
    bool minimizeLossRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Random biased removal: weighted random selection favoring low-efficiency packages
     */
    bool randomBiasedRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Probabilistic greedy removal: stochastic selection based on inefficiency
     */
    bool probabilisticGreedyRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Temperature-based removal: simulated annealing inspired approach
     */
    bool temperatureBiasedRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Max weight removal: removes the heaviest package
     */
    bool maxWeightRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    /**
     * @brief Min efficiency removal: removes package with lowest efficiency
     */
    bool minEfficiencyRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    // Helper functions
    
    /**
     * @brief Computes efficiency as benefit/weight ratio
     */
    double computeEfficiency(const Package* pkg) const noexcept;

    /**
     * @brief Computes a composite removal score considering multiple factors
     */
    double computeRemovalScore(const Package* pkg, int dependents, double efficiency) const noexcept;

    std::mt19937 m_generator;
};

#endif // METAHEURISTICHELPER_H