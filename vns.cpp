#include "vns.h"

#include <chrono>
#include <limits>
#include <algorithm>
#include <iterator>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

VNS::VNS(double maxTime) : m_maxTime(maxTime) {}

VNS::VNS(double maxTime, unsigned int seed) : m_maxTime(maxTime), m_helper(seed) {}

Bag* VNS::run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
              Algorithm::LOCAL_SEARCH localSearchMethod,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VNS);
    bestBag->setLocalSearch(localSearchMethod);

    int k = 1;
    const int k_max = 5;
    bestBag->setMetaheuristicParameters("k_max=" + std::to_string(k_max));

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (k <= k_max) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }
        
        Bag* shakenBag = shake(*bestBag, k, allPackages, bagSize * 2, dependencyGraph);
        m_localSearch.run(*shakenBag, bagSize, allPackages, localSearchMethod, dependencyGraph);
        m_helper.removePackagesToFit(*shakenBag, bagSize, dependencyGraph);

        if (shakenBag->getBenefit() > bestBag->getBenefit()) {
            delete bestBag;
            bestBag = shakenBag;
            k = 1;
        } else {
            delete shakenBag;
            k++;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());
    return bestBag;
}

Bag* VNS::shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize,
                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    Bag* newBag = new Bag(currentBag);
    const auto& packagesInBag = newBag->getPackages();

    for (int i = 0; i < k && !packagesInBag.empty(); ++i) {
        int offset = m_helper.randomNumberInt(0, packagesInBag.size() - 1);
        auto it = packagesInBag.begin();
        std::advance(it, offset);
        const Package* packageToRemove = *it;
        const auto& deps = dependencyGraph.at(packageToRemove);
        newBag->removePackage(*packageToRemove, deps);
    }

    std::vector<Package*> packagesOutside;
    packagesOutside.reserve(allPackages.size());
    const auto& currentPackagesInBag = newBag->getPackages();
    for (Package* p : allPackages) {
        if (currentPackagesInBag.count(p) == 0) {
            packagesOutside.push_back(p);
        }
    }

    for (int i = 0; i < k && !packagesOutside.empty(); ++i) {
        int index = m_helper.randomNumberInt(0, packagesOutside.size() - 1);
        Package* packageToAdd = packagesOutside[index];
        const auto& deps = dependencyGraph.at(packageToAdd);
        if (newBag->canAddPackage(*packageToAdd, bagSize, deps)) {
            newBag->addPackage(*packageToAdd, deps);
        }
        packagesOutside.erase(packagesOutside.begin() + index);
    }
    return newBag;
}