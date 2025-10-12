#include "grasp.h"

#include <chrono>
#include <limits>
#include <algorithm>
#include <iterator>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

GRASP::GRASP(double maxTime) : m_maxTime(maxTime) {}

GRASP::GRASP(double maxTime, unsigned int seed) : m_maxTime(maxTime), m_generator(seed) {}

Bag* GRASP::run(int bagSize, const std::vector<Bag*>& initialBags, const std::vector<Package*>& allPackages,
              Algorithm::LOCAL_SEARCH localSearchMethod,
              const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    Bag* bestBag = nullptr;
    if (!initialBags.empty()) {
        bestBag = new Bag(*initialBags[0]);
    } else {
        bestBag = new Bag(Algorithm::ALGORITHM_TYPE::GRASP, "0");
    }
    
    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::GRASP);
    bestBag->setLocalSearch(localSearchMethod);

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    for (Bag* initialBag : initialBags) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }

        Bag* currentBag = new Bag(*initialBag);
        localSearch(*currentBag, bagSize, allPackages, localSearchMethod, dependencyGraph);

        if (currentBag->getBenefit() > bestBag->getBenefit()) {
            delete bestBag;
            bestBag = currentBag;
        } else {
            delete currentBag;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());
    return bestBag;
}

Bag* GRASP::construction(int bagSize, const std::vector<Package*>& allPackages,
                       const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    Bag* newBag = new Bag(Algorithm::ALGORITHM_TYPE::GRASP, "0");
    std::vector<Package*> candidatePackages = allPackages;
    
    while(!candidatePackages.empty()){
        std::vector<Package*> rcl;
        // Create a Restricted Candidate List (RCL)
        int rcl_size = std::min((int)candidatePackages.size(), 5);
        for(int i = 0; i < rcl_size; ++i){
            rcl.push_back(candidatePackages[i]);
        }

        int index = randomNumberInt(0, rcl.size() - 1);
        Package* packageToAdd = rcl[index];

        const auto& deps = dependencyGraph.at(packageToAdd);
        if (newBag->canAddPackage(*packageToAdd, bagSize, deps)) {
            newBag->addPackage(*packageToAdd, deps);
        }
        
        // Optimized removal of the selected package from the candidate list
        for(size_t i = 0; i < candidatePackages.size(); ++i){
            if(candidatePackages[i] == packageToAdd){
                std::swap(candidatePackages[i], candidatePackages.back());
                candidatePackages.pop_back();
                break;
            }
        }
    }

    return newBag;
}

bool GRASP::localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                      Algorithm::LOCAL_SEARCH localSearchMethod,
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) {
    bool improved = false;
    bool localOptimum = false;
    while (!localOptimum) {
        bool improvement_found_this_iteration = false;
        if (localSearchMethod == Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT) {
            improvement_found_this_iteration = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages, dependencyGraph);
        }

        if (improvement_found_this_iteration) {
            improved = true;
        } else {
            localOptimum = true;
        }
    }
    return improved;
}

bool GRASP::exploreSwapNeighborhoodBestImprovement(
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

bool GRASP::evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const
{
    int newBenefit = currentBenefit - packageIn->getBenefit() + packageOut->getBenefit();
    benefitIncrease = newBenefit - currentBenefit;

    if (benefitIncrease <= 0) return false;

    if (!currentBag.canSwap(*packageIn, *packageOut, bagSize)) return false;

    return true;
}

int GRASP::randomNumberInt(int min, int max)
{
    if (min > max) {
        return min;
    }
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(m_generator);
}