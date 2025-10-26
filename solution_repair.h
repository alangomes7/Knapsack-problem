#ifndef SOLUTION_REPAIR_H
#define SOLUTION_REPAIR_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <random> // <-- ADDED: For std::mt19937

class Bag;
class Package;
class Dependency;

namespace SOLUTION_REPAIR {

enum class FEASIBILITY_STRATEGY {
    SMART,
    TEMPERATURE_BIASED,
    PROBABILISTIC_GREEDY,
};

/**
 * @brief Validates and, if necessary, repairs a Bag.
 *
 * This function first validates the Bag's internal state. If invalid,
 * it tests three parallel repair strategies (SMART, PROBABILISTIC_GREEDY,
 * TEMPERATURE_BIASED) on copies of the Bag. The original Bag is then
 * replaced with the best (highest benefit) feasible result.
 *
 * @param bag The Bag to validate and repair.
 * @param maxCapacity The maximum allowed capacity.
 * @param dependencyGraph The dependency graph.
 * @param seed The seed for the random number generator for reproducible results.
 * @return true if the Bag is valid after the operation, false otherwise.
 */
bool repair(
    Bag& bag,
    int maxCapacity,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    unsigned int seed
);

std::string toString(FEASIBILITY_STRATEGY feasibilityStrategy);

} // namespace SOLUTION_REPAIR

#endif // SOLUTION_REPAIR_H