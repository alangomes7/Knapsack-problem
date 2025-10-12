#ifndef METAHEURISTICHELPER_H
#define METAHEURISTICHELPER_H

#include <vector>
#include <unordered_map>
#include <random>

// Forward declarations
class Bag;
class Package;
class Dependency;

class MetaheuristicHelper {
public:
    MetaheuristicHelper();
    explicit MetaheuristicHelper(unsigned int seed);

    bool removePackagesToFit(Bag& bag, int maxCapacity, const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);
    int randomNumberInt(int min, int max);

private:
    std::mt19937 m_generator;
};

#endif // METAHEURISTICHELPER_H