#include "vnd.h"

#include <chrono>
#include <limits>
#include <algorithm>

#include "bag.h"
#include "package.h"
#include "dependency.h"

VND::VND(double maxTime) : m_maxTime(maxTime) {}

Bag* VND::run(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::VND);
    bestBag->setLocalSearch(Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT);
    bool improved = true;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (improved) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }

        improved = exploreSwapNeighborhoodBestImprovement(*bestBag, bagSize * 1.5, allPackages, dependencyGraph);
        removePackagesToFit(*bestBag, bagSize, dependencyGraph);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());
    return bestBag;
}

bool VND::exploreSwapNeighborhoodBestImprovement(
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

bool VND::evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const
{
    int newBenefit = currentBenefit - packageIn->getBenefit() + packageOut->getBenefit();
    benefitIncrease = newBenefit - currentBenefit;

    if (benefitIncrease <= 0) return false;

    if (!currentBag.canSwap(*packageIn, *packageOut, bagSize)) return false;

    return true;
}

bool VND::removePackagesToFit(Bag& bag, int maxCapacity, const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
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