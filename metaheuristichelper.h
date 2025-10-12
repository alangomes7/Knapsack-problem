#ifndef METAHEURISTICHELPER_H
#define METAHEURISTICHELPER_H

#include <vector>
#include <unordered_map>
#include <random>
#include <optional>

class Bag;
class Package;
class Dependency;

/**
 * @brief Utility class offering advanced helper methods for metaheuristics solving
 *        the knapsack problem with dependencies.
 *
 * It provides multiple strategies to restore feasibility and balance
 * between maximizing total benefit and respecting capacity limits.
 */
class MetaheuristicHelper {
public:
    enum class RemovalStrategy {
        SMART_REMOVAL,
        GREEDY_REMOVAL,
        MAXIMIZE_BENEFIT,
        MINIMIZE_LOSS,
        RANDOM_BIASED,
        BALANCED,
        PROB_GREEDY,        ///< Probabilistic greedy removal (stochastic dominance)
        TEMPERATURE_REMOVAL ///< Temperature-based biased removal
    };

    explicit MetaheuristicHelper(unsigned int seed = std::random_device{}());

    bool makeItFeasible(Bag& bag, int maxCapacity,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool makeItFeasibleAdvanced(Bag& bag, int maxCapacity,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        RemovalStrategy strategy);

    bool makeItFeasibleWithAllStrategies(Bag& bag, int maxCapacity,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    bool makeItFeasibleWithStrategy(Bag& bag, int maxCapacity,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
        int strategyIndex);

    int getStrategyCount() const noexcept;

    int randomNumberInt(int min, int max);
    double randomNumberDouble(double min, double max);

private:
    // Core removal strategies
    bool smartRemoval(Bag&, int, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    bool greedyRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    bool maximizeBenefitRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    bool minimizeLossRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    bool randomBiasedRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    // New enhanced methods
    bool probabilisticGreedyRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    bool temperatureBiasedRemoval(Bag&, int,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>&);

    // Helpers
    double computeEfficiency(const Package* pkg) const noexcept;
    double computeRemovalScore(const Package* pkg, int dependents, double efficiency) const noexcept;

    std::mt19937 m_generator;
    int m_lastUsedStrategy = 0;
};

#endif // METAHEURISTICHELPER_H
