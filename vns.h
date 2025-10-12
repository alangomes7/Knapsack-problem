#ifndef VNS_H
#define VNS_H

#include <vector>
#include <random>
#include <unordered_map>
#include "algorithm.h"
#include "localsearch.h"
#include "metaheuristichelper.h"

// Forward declarations
class Bag;
class Package;
class Dependency;
class Algorithm;

class VNS {
public:
    explicit VNS(double maxTime);
    VNS(double maxTime, unsigned int seed);

    Bag* run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    Bag* shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize,
               const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    
    const double m_maxTime; ///< Timeout in seconds.
    LocalSearch m_localSearch;
    MetaheuristicHelper m_helper;
};

#endif // VNS_H