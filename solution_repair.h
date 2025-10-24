#ifndef SOLUTION_REPAIR_H
#define SOLUTION_REPAIR_H

#include <vector>
#include <unordered_map>
#include <memory>

// Forward declarations
class Bag;
class Package;
class Dependency;
// Removed 'class RandomProvider;' as it's a namespace

/**
 * @brief Namespace that provides a function to repair an infeasible solution.
 *
 * You just call `SolutionRepair::run(...)` passing all necessary parameters.
 */
namespace SolutionRepair {

enum class FeasibilityStrategy {
    SMART,
    TEMPERATURE_BIASED,
    PROBABILISTIC_GREEDY,
};

/**
 * @brief Repairs a Bag to satisfy maxCapacity by removing packages intelligently.
 * @param bag The bag to repair (modified in-place)
 * @param maxCapacity Capacity constraint
 * @param dependencyGraph Dependency map
 * @return true if a feasible solution was found, false otherwise
 */
bool repair(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
);

} // namespace SolutionRepair

#endif // SOLUTION_REPAIR_H