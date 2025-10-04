#include "algorithm.h"

#include <random>
#include <chrono>
#include <utility>
#include <algorithm>
#include <iterator>
#include <limits>

#include "bag.h"
#include "package.h"
#include "dependency.h"

Algorithm::Algorithm(double maxTime)
    : m_maxTime(maxTime) {
}

// Executes all strategies
std::vector<Bag*> Algorithm::run(ALGORITHM_TYPE algorithm,int bagSize,
                       const std::vector<Package *> &packages,
                       const std::vector<Dependency *> &dependencies,
                       const std::string& timestamp)
{
    m_timestamp = timestamp;
    std::vector<Bag*> resultBag;
    resultBag.reserve(10); // Pre-allocate expected size
    
    resultBag.push_back(randomBag(bagSize, packages, dependencies));
    std::vector<Bag*> bagsGreedy = greedyBag(bagSize, packages, dependencies);
    std::vector<Bag*> bagsRandomGreedy = randomGreedy(bagSize, packages, dependencies);

    resultBag.insert(resultBag.end(), bagsGreedy.begin(), bagsGreedy.end());
    resultBag.insert(resultBag.end(), bagsRandomGreedy.begin(), bagsRandomGreedy.end());

    // Find the best solution from the constructive heuristics to use as a starting point
    Bag* bestInitialBag = nullptr;
    int bestInitialBenefit = -1;
    for (Bag* b : resultBag) {
        if (b->getBenefit() > bestInitialBenefit) {
            bestInitialBenefit = b->getBenefit();
            bestInitialBag = b;
        }
    }

    // Run metaheuristics using the best initial solution
    if (bestInitialBag) {
        resultBag.push_back(vndBag(bagSize, bestInitialBag, packages));
        resultBag.push_back(vnsBag(bagSize, bestInitialBag, packages, LOCAL_SEARCH::FIRST_IMPROVEMENT));
        resultBag.push_back(vnsBag(bagSize, bestInitialBag, packages, LOCAL_SEARCH::BEST_IMPROVEMENT));
        resultBag.push_back(vnsBag(bagSize, bestInitialBag, packages, LOCAL_SEARCH::RANDOM_IMPROVEMENT));
    }

    return resultBag;
}

std::string Algorithm::toString(ALGORITHM_TYPE algorithm) const {
    switch (algorithm) {
        case ALGORITHM_TYPE::RANDOM:
            return "RANDOM";
        case ALGORITHM_TYPE::GREEDY_Package_Benefit:
            return "GREEDY -> Package: Benefit";
        case ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio:
            return "GREEDY -> Package: Benefit Ratio";
        case ALGORITHM_TYPE::GREEDY_Package_Size:
            return "GREEDY -> Package: Size";
        case ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit:
            return "RANDOM_GREEDY (Package: Benefit)";
        case ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio:
            return "RANDOM_GREEDY (Package: Benefit Ratio)";
        case ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size:
            return "RANDOM_GREEDY (Package: Size)";
        case ALGORITHM_TYPE::VND:
            return "VND";
        case ALGORITHM_TYPE::VNS:
            return "VNS";
        default:
            return "NONE";
    }
}

std::string Algorithm::toString(LOCAL_SEARCH localSearch) const
{
    switch (localSearch)
    {
    case LOCAL_SEARCH::FIRST_IMPROVEMENT:
        return "First Improvement";
    case LOCAL_SEARCH::BEST_IMPROVEMENT:
        return "Best Improvement";
    case LOCAL_SEARCH::RANDOM_IMPROVEMENT:
            return "Random Improvement";
    default:
        return "None";
    }
}

// ===================================================================
// == Private Algorithm Implementations
// ===================================================================

Bag* Algorithm::randomBag(int bagSize, const std::vector<Package*>& packages,
                          const std::vector<Dependency*>& dependencies) {
    auto mutablePackages = packages;
    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickRandomPackage(pkgs);
    };
    return fillBagWithStrategy(bagSize, mutablePackages, pickStrategy, ALGORITHM_TYPE::RANDOM);
}

Bag* Algorithm::vndBag(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages)
{
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(ALGORITHM_TYPE::VND);
    bestBag->setLocalSearch(LOCAL_SEARCH::BEST_IMPROVEMENT);
    bool improved = true;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (improved) {
        // Stop if the time limit is exceeded
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }

        improved = exploreSwapNeighborhoodBestImprovement(*bestBag, bagSize * 1.5, allPackages);
        removePackagesToFit(*bestBag, bagSize);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBag->setAlgorithmTime(elapsed_seconds.count());
    return bestBag;
}

Bag* Algorithm::vnsBag(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages, LOCAL_SEARCH localSearchMethod) {
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(ALGORITHM_TYPE::VNS);
    bestBag->setLocalSearch(localSearchMethod);

    int k = 1;
    const int k_max = 5;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (k <= k_max) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }
        
        Bag* shakenBag = shake(*bestBag, k, allPackages, bagSize * 2);

        localSearch(*shakenBag, bagSize, allPackages, localSearchMethod);
        removePackagesToFit(*shakenBag, bagSize);

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

Bag* Algorithm::shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize) {
    Bag* newBag = new Bag(currentBag);
    const auto& packagesInBag = newBag->getPackages();

    // Randomly remove k packages
    for (int i = 0; i < k && !packagesInBag.empty(); ++i) {
        int offset = randomNumberInt(0, packagesInBag.size() - 1);
        auto it = packagesInBag.begin();
        std::advance(it, offset);
        const Package* packageToRemove = *it;
        newBag->removePackage(*packageToRemove);
    }

    // Get packages not currently in the bag
    std::vector<Package*> packagesOutside;
    packagesOutside.reserve(allPackages.size()); // Pre-allocate
    const auto& currentPackagesInBag = newBag->getPackages();
    for (Package* p : allPackages) {
        if (currentPackagesInBag.count(p) == 0) {
            packagesOutside.push_back(p);
        }
    }

    // Randomly add k valid packages from the outside list
    for (int i = 0; i < k && !packagesOutside.empty(); ++i) {
        int index = randomNumberInt(0, packagesOutside.size() - 1);
        Package* packageToAdd = packagesOutside[index];
        if (newBag->canAddPackage(*packageToAdd, bagSize)) {
            newBag->addPackage(*packageToAdd);
        }
        packagesOutside.erase(packagesOutside.begin() + index);
    }

    return newBag;
}

bool Algorithm::localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages, LOCAL_SEARCH localSearchMethod) {
    bool improved = false;
    bool localOptimum = false;
    while (!localOptimum) {
        bool improvement_found_this_iteration = false;
        switch (localSearchMethod)
        {
            case LOCAL_SEARCH::BEST_IMPROVEMENT:
                improvement_found_this_iteration = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages);
                break;
            case LOCAL_SEARCH::RANDOM_IMPROVEMENT:
                improvement_found_this_iteration = exploreSwapNeighborhoodRandomImprovement(currentBag, bagSize, allPackages);
                break;
            default:
                improvement_found_this_iteration = exploreSwapNeighborhoodFirstImprovement(currentBag, bagSize, allPackages);
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

bool Algorithm::exploreSwapNeighborhoodFirstImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    // Build outside list
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
                currentBag.removePackage(*packageIn);
                currentBag.addPackage(*packageOut);
                return true; // stop at first improvement
            }
        }
    }
    return false;
}

bool Algorithm::exploreSwapNeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    // Build outside list
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
        currentBag.removePackage(*bestPackageIn);
        currentBag.addPackage(*bestPackageOut);
        return true;
    }
    return false;
}

bool Algorithm::exploreSwapNeighborhoodRandomImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages)
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
                neighbor.removePackage(*packageIn);
                neighbor.addPackage(*packageOut);
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

bool Algorithm::evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const
{
    // Compute new benefit incrementally (O(1))
    int newBenefit = currentBenefit - packageIn->getBenefit() + packageOut->getBenefit();
    benefitIncrease = newBenefit - currentBenefit;

    if (benefitIncrease <= 0) return false;

    // Check feasibility with capacity + dependencies
    if (!currentBag.canSwap(*packageIn, *packageOut, bagSize)) return false;

    return true;
}

std::vector<Bag *> Algorithm::greedyBag(int bagSize, const std::vector<Package *> &packages,
                                        const std::vector<Dependency *> &dependencies)
{
    std::vector<Bag*> bags;
    bags.reserve(3); // Pre-allocate
    
    std::vector<Package *> sortedByRatio = sortedPackagesByBenefitToSizeRatio(packages);
    std::vector<Package *> sortedByBenefit = sortedPackagesByBenefit(packages);
    std::vector<Package *> sortedBySize = sortedPackagesBySize(packages);

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickTopPackage(pkgs);
    };

    bags.push_back(fillBagWithStrategy(bagSize, sortedByRatio, pickStrategy,
                                       ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio));
    bags.push_back(fillBagWithStrategy(bagSize, sortedByBenefit, pickStrategy,
                                       ALGORITHM_TYPE::GREEDY_Package_Benefit));
    bags.push_back(fillBagWithStrategy(bagSize, sortedBySize, pickStrategy,
                                       ALGORITHM_TYPE::GREEDY_Package_Size));
    return bags;
}

std::vector<Bag*> Algorithm::randomGreedy(int bagSize, const std::vector<Package*>& packages,
                                          const std::vector<Dependency*>& dependencies) {
    std::vector<Bag*> bags;
    bags.reserve(3); // Pre-allocate
    
    std::vector<Package *> sortedByRatio = sortedPackagesByBenefitToSizeRatio(packages);
    std::vector<Package *> sortedByBenefit = sortedPackagesByBenefit(packages);
    std::vector<Package *> sortedBySize = sortedPackagesBySize(packages);

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        const int candidatePoolSize = 10;
        return this->pickSemiRandomPackage(pkgs, candidatePoolSize);
    };

    bags.push_back(fillBagWithStrategy(bagSize, sortedByBenefit, pickStrategy,
                                       ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit));
    bags.push_back(fillBagWithStrategy(bagSize, sortedByRatio, pickStrategy,
                                       ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio));
    bags.push_back(fillBagWithStrategy(bagSize, sortedBySize, pickStrategy,
                                       ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size));
    return bags;
}

// ===================================================================
// == Core Logic and Utilities
// ===================================================================

Package* Algorithm::pickRandomPackage(std::vector<Package*>& packageList) {
    if (packageList.empty()) {
        return nullptr;
    }
    int index = randomNumberInt(0, packageList.size() - 1);
    Package* pickedPackage = packageList[index];
    packageList.erase(packageList.begin() + index);
    return pickedPackage;
}

Package* Algorithm::pickTopPackage(std::vector<Package*>& packageList) {
    if (packageList.empty()) {
        return nullptr;
    }
    Package* pickedPackage = packageList.front();
    packageList.erase(packageList.begin());
    return pickedPackage;
}

Package* Algorithm::pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize) {
    if (packageList.empty()) {
        return nullptr;
    }
    int candidatePoolSize = std::min(poolSize, static_cast<int>(packageList.size()));
    int index = randomNumberInt(0, candidatePoolSize - 1);
    Package* pickedPackage = packageList[index];
    packageList.erase(packageList.begin() + index);
    return pickedPackage;
}

Bag* Algorithm::fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                                      std::function<Package*(std::vector<Package*>&)> pickStrategy,
                                      ALGORITHM_TYPE type) {
    auto bag = new Bag(type, m_timestamp);
    if (packages.empty()) {
        return bag;
    }

    auto compatibilityCache = std::unordered_map<const Package*, bool>();
    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);
    
    while (!packages.empty()) {
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }
        Package* packageToAdd = pickStrategy(packages);
        if (!packageToAdd) {
            break; 
        }

        if (canPackageBeAdded(*bag, *packageToAdd, bagSize, compatibilityCache)) {
            bag->addPackage(*packageToAdd);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bag->setAlgorithmTime(elapsed_seconds.count());
    return bag;
}

bool Algorithm::removePackagesToFit(Bag& bag, int maxCapacity) {
    while (bag.getSize() > maxCapacity) {
        const auto& packagesInBag = bag.getPackages();

        if (packagesInBag.empty()) {
            return false;
        }

        const Package* worstPackageToRemove = nullptr;
        double minRatio = std::numeric_limits<double>::max();

        for (const Package* package : packagesInBag) {
            int packageDependenciesSize = package->getDependenciesSize();
            double ratio;

            if (packageDependenciesSize <= 0) {
                ratio = (package->getBenefit() <= 0) ? -1.0 : std::numeric_limits<double>::max();
            } else {
                ratio = static_cast<double>(package->getBenefit()) / packageDependenciesSize;
            }

            if (ratio < minRatio) {
                minRatio = ratio;
                worstPackageToRemove = package;
            }
        }

        if (worstPackageToRemove) {
            bag.removePackage(*worstPackageToRemove);
        } else {
            return false;
        }
    }

    return true;
}

std::vector<Package *> Algorithm::sortedPackagesByBenefit(const std::vector<Package *> &packages)
{
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        return a->getBenefit() > b->getBenefit();
    });
    return sortedList;
}

std::vector<Package*> Algorithm::sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages) {
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        double ratio_a = (a->getDependenciesSize() > 0) ? static_cast<double>(a->getBenefit()) / a->getDependenciesSize() : a->getBenefit();
        double ratio_b = (b->getDependenciesSize() > 0) ? static_cast<double>(b->getBenefit()) / b->getDependenciesSize() : b->getBenefit();
        return ratio_a > ratio_b;
    });
    return sortedList;
}

std::vector<Package*> Algorithm::sortedPackagesBySize(const std::vector<Package*>& packages) {
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        return a->getDependenciesSize() < b->getDependenciesSize();
    });
    return sortedList;
}

int Algorithm::randomNumberInt(int min, int max)
{
    static std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());
    if (min > max) {
        return min;
    }
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(generator);
}

void Algorithm::precomputeDependencyGraph(const std::vector<Package*>& packages,
                                          const std::vector<Dependency*>& dependencies)
{
    // Stub for potential optimization.
}

bool Algorithm::canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                                  std::unordered_map<const Package*, bool>& cache) {
    auto it = cache.find(&package);
    if (it != cache.end()) {
        return it->second;
    }
    bool result = bag.canAddPackage(package, maxCapacity);
    cache[&package] = result;
    return result;
}