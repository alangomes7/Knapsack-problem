#include "metaheuristichelper.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include <limits>
#include <algorithm>

MetaheuristicHelper::MetaheuristicHelper() : m_generator(std::random_device{}()) {}

MetaheuristicHelper::MetaheuristicHelper(unsigned int seed) : m_generator(seed) {}

bool MetaheuristicHelper::removePackagesToFit(Bag& bag, int maxCapacity, const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    while (bag.getSize() > maxCapacity) {
        const auto& packagesInBag = bag.getPackages();
        if (packagesInBag.empty()) return false;

        const Package* worstPackageToRemove = nullptr;
        double minRatio = std::numeric_limits<double>::max();

        for (const Package* package : packagesInBag) {
            int packageDependenciesSize = package->getDependenciesSize();
            double ratio = (packageDependenciesSize <= 0) ?
                           ((package->getBenefit() <= 0) ? -1.0 : std::numeric_limits<double>::max()) :
                           static_cast<double>(package->getBenefit()) / packageDependenciesSize;

            if (ratio < minRatio) {
                minRatio = ratio;
                worstPackageToRemove = package;
            }
        }

        if (worstPackageToRemove) {
            const auto& deps = dependencyGraph.at(worstPackageToRemove);
            bag.removePackage(*worstPackageToRemove, deps);
        } else {
            return false;
        }
    }
    return true;
}

int MetaheuristicHelper::randomNumberInt(int min, int max)
{
    if (min > max) {
        return min;
    }
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(m_generator);
}