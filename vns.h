#ifndef VNS_H
#define VNS_H

#include <vector>
#include <random>
#include <unordered_map>
#include <memory>
#include "algorithm.h"
#include "search_engine.h"

// Forward declarations
class Bag;
class Package;
class Dependency;

/**
 * @brief Variable Neighborhood Search (VNS) metaheuristic
 */
class VNS {
public:
    VNS(double maxTime, unsigned int seed);

    /**
     * @brief Run VNS to improve an initial bag
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

#endif // VNS_H
