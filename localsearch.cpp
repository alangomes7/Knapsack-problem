#include "localsearch.h"
#include "bag.h"
#include "package.h"
#include "dependency.h"
#include <random>

bool LocalSearch::run(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    Algorithm::LOCAL_SEARCH localSearchMethod,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    bool improved = false;
    while (true) {
        bool improvement_found = false;
        switch (localSearchMethod) {
            case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT:
                improvement_found = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
            case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT:
                improvement_found = exploreSwapNeighborhoodRandomImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
            default:
                improvement_found = exploreSwapNeighborhoodFirstImprovement(currentBag, bagSize, allPackages, dependencyGraph);
                break;
        }
        if (!improvement_found) break;
        improved = true;
    }
    return improved;
}

bool LocalSearch::exploreSwapNeighborhoodFirstImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInBagVec(packagesInBag.begin(), packagesInBag.end());
    const int currentBenefit = currentBag.getBenefit();
    int delta;

    for (const Package* packageIn : packagesInBagVec) {
        const auto& depsIn = dependencyGraph.at(packageIn);
        for (Package* packageOut : packagesOutsideBag) {
            if (evaluateSwap(currentBag, packageIn, packageOut, bagSize, currentBenefit, delta)) {
                const auto& depsOut = dependencyGraph.at(packageOut);
                currentBag.removePackage(*packageIn, depsIn);
                currentBag.addPackage(*packageOut, depsOut);
                return true;
            }
        }
    }
    return false;
}

bool LocalSearch::exploreSwapNeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInBagVec(packagesInBag.begin(), packagesInBag.end());
    const int currentBenefit = currentBag.getBenefit();

    const Package* bestIn = nullptr;
    Package* bestOut = nullptr;
    int maxDelta = 0, delta = 0;

    for (const Package* packageIn : packagesInBagVec) {
        const auto& depsIn = dependencyGraph.at(packageIn);
        for (Package* packageOut : packagesOutsideBag) {
            if (evaluateSwap(currentBag, packageIn, packageOut, bagSize, currentBenefit, delta)) {
                if (delta > maxDelta) {
                    maxDelta = delta;
                    bestIn = packageIn;
                    bestOut = packageOut;
                }
            }
        }
    }

    if (bestIn && bestOut) {
        const auto& depsIn = dependencyGraph.at(bestIn);
        const auto& depsOut = dependencyGraph.at(bestOut);
        currentBag.removePackage(*bestIn, depsIn);
        currentBag.addPackage(*bestOut, depsOut);
        return true;
    }
    return false;
}

bool LocalSearch::exploreSwapNeighborhoodRandomImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> packagesOutsideBag;
    buildOutsidePackages(packagesInBag, allPackages, packagesOutsideBag);
    if (packagesOutsideBag.empty()) return false;

    std::vector<const Package*> packagesInBagVec(packagesInBag.begin(), packagesInBag.end());
    const int currentBenefit = currentBag.getBenefit();

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> disIn(0, packagesInBagVec.size() - 1);
    std::uniform_int_distribution<size_t> disOut(0, packagesOutsideBag.size() - 1);

    const int numAttempts = std::min(200, (int)packagesInBagVec.size() * (int)packagesOutsideBag.size());
    int delta = 0;

    for (int i = 0; i < numAttempts; ++i) {
        const Package* packageIn = packagesInBagVec[disIn(gen)];
        Package* packageOut = packagesOutsideBag[disOut(gen)];
        if (evaluateSwap(currentBag, packageIn, packageOut, bagSize, currentBenefit, delta)) {
            const auto& depsIn = dependencyGraph.at(packageIn);
            const auto& depsOut = dependencyGraph.at(packageOut);
            currentBag.removePackage(*packageIn, depsIn);
            currentBag.addPackage(*packageOut, depsOut);
            return true;
        }
    }
    return false;
}

bool LocalSearch::evaluateSwap(
    const Bag& currentBag, const Package* packageIn, Package* packageOut,
    int bagSize, int currentBenefit, int& benefitIncrease) const
{
    const int delta = packageOut->getBenefit() - packageIn->getBenefit();
    if (delta <= 0) return false;
    if (!currentBag.canSwap(*packageIn, *packageOut, bagSize)) return false;
    benefitIncrease = delta;
    return true;
}

// --- Helper to build outside packages (reused by all methods)
void LocalSearch::buildOutsidePackages(
    const std::unordered_set<const Package*>& packagesInBag,
    const std::vector<Package*>& allPackages,
    std::vector<Package*>& packagesOutsideBag)
{
    packagesOutsideBag.clear();
    packagesOutsideBag.reserve(allPackages.size() - packagesInBag.size());
    for (Package* p : allPackages)
        if (!packagesInBag.count(p))
            packagesOutsideBag.push_back(p);
}