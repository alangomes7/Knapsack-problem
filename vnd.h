#ifndef VND_H
#define VND_H

#include <vector>
#include <unordered_map>
#include <memory>

#include "algorithm.h"
#include "search_engine.h"

// Forward declarations
class Bag;
class Package;
class Dependency;

/**
 * @brief Parallel-friendly Variable Neighborhood Descent (VND)
 */
class VND {
public:
    explicit VND(double maxTime, unsigned int seed);

    /**
     * @brief Runs the VND algorithm
     * @param bagSize Maximum capacity
     * @param initialBag Initial bag
     * @param allPackages All available packages
     * @param dependencyGraph Precomputed dependencies
     * @return Unique pointer to the best solution
     */
    std::unique_ptr<Bag> run(
        int bagSize,
        const Bag* initialBag,
        const std::vector<Package*>& allPackages,
        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
    );

private:
    const double m_maxTime;
    SearchEngine m_searchEngine;
};

#endif // VND_H
