#ifndef VND_H
#define VND_H

#include <vector>
#include <unordered_map>
#include "algorithm.h"
#include "localsearch.h"
#include "metaheuristichelper.h"

// Forward declarations to avoid circular dependencies
class Bag;
class Package;
class Dependency;
class Algorithm;

class VND {
public:
    explicit VND(double maxTime);

    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    const double m_maxTime; ///< Timeout in seconds.
    LocalSearch m_localSearch;
    MetaheuristicHelper m_helper;
};

#endif // VND_H