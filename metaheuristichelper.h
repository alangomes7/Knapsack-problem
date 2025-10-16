#ifndef METAHEURISTICHELPER_H
#define METAHEURISTICHELPER_H

#include <vector>
#include <unordered_map>
#include <random>

class Bag;
class Package;
class Dependency;

/**
 * @brief Utility class offering helper methods for metaheuristics solving
 *        the knapsack problem with dependencies.
 *
 * This simplified version includes only:
 *  - smartRemoval()
 *  - temperatureBiasedRemoval()
 *  - probabilisticGreedyRemoval()
 */
class MetaheuristicHelper {
public:
    explicit MetaheuristicHelper(unsigned int seed = std::random_device{}());

    /**
     * @brief Makes the bag feasible by removing packages until within capacity.
     */
    bool makeItFeasible(
        Bag& bag, int maxCapacity,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    /**
     * @brief Returns the number of available removal strategies.
     */
    int getStrategyCount() const noexcept;

    int getSeed() const;

    int randomNumberInt(int min, int max);
    double randomNumberDouble(double min, double max);

private:
    bool removeOnePackageWithStrategy(
        Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&, int strategyIndex);

    // --- Active strategies ---
    bool smartRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);
    bool probabilisticGreedyRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);
    bool temperatureBiasedRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    // --- Helpers ---
    double computeEfficiency(const Package* pkg) const noexcept;
    double computeRemovalScore(const Package* pkg, int dependents, double efficiency) const noexcept;

    int m_seed;
    std::mt19937 m_generator;
};

#endif // METAHEURISTICHELPER_H
