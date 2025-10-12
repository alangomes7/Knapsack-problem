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

VNS::VNS(double maxTime, unsigned int seed) : m_maxTime(maxTime), m_generator(seed) {}

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
        localSearch(*shakenBag, bagSize, allPackages, localSearchMethod, dependencyGraph);
        removePackagesToFit(*shakenBag, bagSize, dependencyGraph);

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
        int offset = randomNumberInt(0, packagesInBag.size() - 1);
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
        int index = randomNumberInt(0, packagesOutside.size() - 1);
        Package* packageToAdd = packagesOutside[index];
        const auto& deps = dependencyGraph.at(packageToAdd);
        if (newBag->canAddPackage(*packageToAdd, bagSize, deps)) {
            newBag->addPackage(*packageToAdd, deps);
        }
        packagesOutside.erase(packagesOutside.begin() + index);
    }
    return newBag;
}

bool VNS::localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                      Algorithm::LOCAL_SEARCH localSearchMethod,
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    bool improved = false;
    bool localOptimum = false;
    while (!localOptimum) {
        bool improvement_found_this_iteration = false;
        switch (localSearchMethod) {
            case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT:
                improvement_found_this_iteration = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
            case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT:
                improvement_found_this_iteration = exploreSwapNeighborhoodRandomImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
            default:
                improvement_found_this_iteration = exploreSwapNeighborhoodFirstImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
        }

        if (improvement_found_this_iteration) {
            improved = true;
        } else {
            localOptimum = true;
        }
    }
    return improved;
}

bool VNS::exploreSwapNeighborhoodFirstImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    packagesOutsideBag.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (packagesInBag.count(p) == 0) packagesOutsideBag.push_back(p);
    }
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInBagVec(packagesInBag.begin(), packagesInBag.end());
    int currentBenefit = currentBag.getBenefit();
    int delta;

    for (const Package* packageIn : packagesInBagVec) {
        for (Package* packageOut : packagesOutsideBag) {
            if (evaluateSwap(currentBag, packageIn, packageOut, bagSize, currentBenefit, delta)) {
                const auto& depsIn = dependencyGraph.at(packageIn);
                currentBag.removePackage(*packageIn, depsIn);
                const auto& depsOut = dependencyGraph.at(packageOut);
                currentBag.addPackage(*packageOut, depsOut);
                return true;
            }
        }
    }
    return false;
}

bool VNS::exploreSwapNeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    packagesOutsideBag.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (packagesInBag.count(p) == 0) packagesOutsideBag.push_back(p);
    }
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInBagVec(packagesInBag.begin(), packagesInBag.end());
    int currentBenefit = currentBag.getBenefit();

    const Package* bestPackageIn = nullptr;
    Package* bestPackageOut = nullptr;
    int maxBenefitIncrease = 0, delta = 0;

    for (const Package* packageIn : packagesInBagVec) {
        for (Package* packageOut : packagesOutsideBag) {
            if (evaluateSwap(currentBag, packageIn, packageOut, bagSize, currentBenefit, delta)) {
                if (delta > maxBenefitIncrease) {
                    maxBenefitIncrease = delta;
                    bestPackageIn = packageIn;
                    bestPackageOut = packageOut;
                }
            }
        }
    }

    if (bestPackageIn && bestPackageOut) {
        const auto& depsIn = dependencyGraph.at(bestPackageIn);
        currentBag.removePackage(*bestPackageIn, depsIn);
        const auto& depsOut = dependencyGraph.at(bestPackageOut);
        currentBag.addPackage(*bestPackageOut, depsOut);
        return true;
    }
    return false;
}

bool VNS::exploreSwapNeighborhoodRandomImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    packagesOutsideBag.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (packagesInBag.count(p) == 0) packagesOutsideBag.push_back(p);
    }
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInBagVec(packagesInBag.begin(), packagesInBag.end());
    int currentBenefit = currentBag.getBenefit();

    const int numAttempts = std::min(200, (int)packagesInBagVec.size() * (int)packagesOutsideBag.size());
    int improvingCount = 0, delta = 0;
    Bag chosenNeighbor = currentBag;

    for (int i = 0; i < numAttempts; ++i) {
        const Package* packageIn = packagesInBagVec[randomNumberInt(0, (int)packagesInBagVec.size() - 1)];
        Package* packageOut = packagesOutsideBag[randomNumberInt(0, (int)packagesOutsideBag.size() - 1)];

        if (evaluateSwap(currentBag, packageIn, packageOut, bagSize, currentBenefit, delta)) {
            improvingCount++;
            if (randomNumberInt(1, improvingCount) == 1) {
                Bag neighbor = currentBag;
                const auto& depsIn = dependencyGraph.at(packageIn);
                neighbor.removePackage(*packageIn, depsIn);
                const auto& depsOut = dependencyGraph.at(packageOut);
                neighbor.addPackage(*packageOut, depsOut);
                chosenNeighbor = std::move(neighbor);
            }
        }
    }

    if (improvingCount > 0) {
        currentBag = std::move(chosenNeighbor);
        return true;
    }
    return false;
}

bool VNS::evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const
{
    int newBenefit = currentBenefit - packageIn->getBenefit() + packageOut->getBenefit();
    benefitIncrease = newBenefit - currentBenefit;

    if (benefitIncrease <= 0) return false;

    if (!currentBag.canSwap(*packageIn, *packageOut, bagSize)) return false;

    return true;
}

bool VNS::removePackagesToFit(Bag& bag, int maxCapacity, const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
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

int VNS::randomNumberInt(int min, int max)
{
    if (min > max) {
        return min;
    }
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(m_generator);
}