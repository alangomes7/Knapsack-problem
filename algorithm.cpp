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
    bool improved = true;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (improved) {
        // Stop if the time limit is exceeded
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }

        improved = exploreSwapNeighborhoodBestImprovement(*bestBag, bagSize, allPackages);
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
    const int k_max = 5; // The maximum neighborhood size for shaking

    auto start_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> max_duration_seconds(m_maxTime);

    while (k <= k_max) {
        // Stop if the time limit is exceeded
        if (std::chrono::high_resolution_clock::now() - start_time > max_duration_seconds) {
            break;
        }
        
        // 1. Shaking: Generate a random solution in the k-th neighborhood
        int extraSize = 2000;
        Bag* shakenBag = shake(*bestBag, k, allPackages, bagSize + extraSize);

        // 2. Local Search: Find the local optimum from the shaken solution
        localSearch(*shakenBag, bagSize, allPackages, localSearchMethod);

        // 3. Repair bag to meet the max_size requirement
        removePackagesToFit(*shakenBag, bagSize);

        // 4. Move: If the new solution is better, update the best solution
        if (shakenBag->getBenefit() > bestBag->getBenefit()) {
            delete bestBag;
            bestBag = shakenBag;
            k = 1; // Reset to the first neighborhood
        } else {
            delete shakenBag;
            k++; // Move to the next neighborhood
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
        // FIX: Cannot use operator[] on an unordered_set.
        // We must advance an iterator a random number of steps to get a random element.
        int offset = randomNumberInt(0, packagesInBag.size() - 1);
        auto it = packagesInBag.begin();
        std::advance(it, offset);
        const Package* packageToRemove = *it;
        newBag->removePackage(*packageToRemove);
    }

    // Get packages not currently in the bag
    std::vector<Package*> packagesOutside;
    const auto& currentPackagesInBag = newBag->getPackages(); // Re-fetch after removals
    for (Package* p : allPackages) {
        // FIX: Use the efficient .count() method instead of a slow nested loop.
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
        // Erase the chosen package to prevent re-selection
        packagesOutside.erase(packagesOutside.begin() + index);
    }

    return newBag;
}

bool Algorithm::localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages, LOCAL_SEARCH localSearchMethod) {
    bool improved = false;
    bool localOptimum = false;
    // Keep searching until no more improvements can be made
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
            default: // First improvement is the default
                improvement_found_this_iteration = exploreSwapNeighborhoodFirstImprovement(currentBag, bagSize, allPackages);
                break;
        }

        if (improvement_found_this_iteration) {
            improved = true;
        } else {
            localOptimum = true; // No improvement found, so we're at a local optimum
        }
    }
    return improved;
}

bool Algorithm::exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages) {
    const auto& packagesInBag = currentBag.getPackages();
    std::vector<Package*> packagesOutsideBag;
    for (Package* p : allPackages) {
        // FIX: Use the efficient .count() method instead of a slow nested loop.
        if (packagesInBag.count(p) == 0) {
            packagesOutsideBag.push_back(p);
        }
    }

    // Create a copy of the packages in the bag to iterate over, avoiding iterator invalidation.
    std::vector<const Package*> packagesInBagCopy(packagesInBag.begin(), packagesInBag.end());

    for (const Package* packageIn : packagesInBagCopy) {
        for (Package* packageOut : packagesOutsideBag) {
            int originalBenefit = currentBag.getBenefit();
            currentBag.removePackage(*packageIn);

            if (currentBag.canAddPackage(*packageOut, bagSize)) {
                currentBag.addPackage(*packageOut);
                if (currentBag.getBenefit() > originalBenefit) {
                    return true; // Improvement found and applied, so return immediately.
                }
                // Backtrack if not an improvement
                currentBag.removePackage(*packageOut);
                currentBag.addPackage(*packageIn);
            } else {
                // Backtrack if packageOut can't be added
                currentBag.addPackage(*packageIn);
            }
        }
    }
    return false; // No improvement found
}


bool Algorithm::exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages) {
    Bag bestNeighborBag = currentBag;
    bool improvementFound = false;

    const auto& packagesInBag = currentBag.getPackages();
    std::vector<Package*> packagesOutsideBag;
    for (Package* p : allPackages) {
        // FIX: Use the efficient .count() method.
        if (packagesInBag.count(p) == 0) {
            packagesOutsideBag.push_back(p);
        }
    }
    
    // Create a copy to iterate over, as the original bag can be modified.
    std::vector<const Package*> packagesInBagCopy(packagesInBag.begin(), packagesInBag.end());

    for (const Package* packageIn : packagesInBagCopy) {
        for (Package* packageOut : packagesOutsideBag) {
            Bag tempBag = currentBag;
            tempBag.removePackage(*packageIn);
            if (tempBag.canAddPackage(*packageOut, bagSize)) {
                tempBag.addPackage(*packageOut);
                if (tempBag.getBenefit() > bestNeighborBag.getBenefit()) {
                    bestNeighborBag = tempBag;
                    improvementFound = true;
                }
            }
        }
    }

    if (improvementFound) {
        currentBag = bestNeighborBag;
    }
    return improvementFound;
}

bool Algorithm::exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages) {
    std::vector<Bag> improvingNeighbors;
    const auto& packagesInBag = currentBag.getPackages();
    std::vector<Package*> packagesOutsideBag;

    for (Package* p : allPackages) {
        // Use the efficient .count() method.
        if (packagesInBag.count(p) == 0) {
            packagesOutsideBag.push_back(p);
        }
    }
    
    std::vector<const Package*> packagesInBagCopy(packagesInBag.begin(), packagesInBag.end());

    // A swap is only possible if there are packages both inside and outside the bag.
    if (packagesInBagCopy.empty() || packagesOutsideBag.empty()) {
        // No possible swaps, so no improvement can be made.
        return false; // <-- FIX: Changed from 'return;' to 'return false;'
    }

    // Define how many random swaps we should attempt. This is a tunable parameter.
    // A higher number explores the neighborhood more thoroughly but takes more time.
    const int numAttempts = 500;

    for (int i = 0; i < numAttempts; ++i) {
        // 1. Randomly select a package from INSIDE the bag using a random index.
        int inIndex = randomNumberInt(0, packagesInBagCopy.size() - 1);
        const Package* packageIn = packagesInBagCopy[inIndex];

        // 2. Randomly select a package from OUTSIDE the bag using a random index.
        int outIndex = randomNumberInt(0, packagesOutsideBag.size() - 1);
        Package* packageOut = packagesOutsideBag[outIndex];
        
        // 3. Test this randomly selected swap.
        Bag tempBag = currentBag;
        tempBag.removePackage(*packageIn);
        
        if (tempBag.canAddPackage(*packageOut, bagSize)) {
            tempBag.addPackage(*packageOut);
            
            // If the swap results in an improvement, add it to the list of candidates.
            if (tempBag.getBenefit() > currentBag.getBenefit()) {
                improvingNeighbors.push_back(tempBag);
            }
        }
    }

    // If we found any valid improvements, pick one at random and apply it.
    if (!improvingNeighbors.empty()) {
        int randomIndex = randomNumberInt(0, improvingNeighbors.size() - 1);
        currentBag = improvingNeighbors[randomIndex];
        return true;
    }

    // If after all attempts no improvements were found, return false.
    return false;
}

std::vector<Bag*> Algorithm::greedyBag(int bagSize, const std::vector<Package*>& packages,
                                       const std::vector<Dependency*>& dependencies) {
    std::vector<Bag*> bags;
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
    
    // This loop continues as long as there are packages to try and time hasn't run out.
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
    // Loop as long as the bag is over its capacity.
    while (bag.getSize() > maxCapacity) {
        const auto& packagesInBag = bag.getPackages();

        // If the bag is over capacity but has no packages, something is wrong. We can't fix it.
        if (packagesInBag.empty()) {
            return false;
        }

        const Package* worstPackageToRemove = nullptr;
        // Initialize the minimum ratio to the highest possible value.
        double minRatio = std::numeric_limits<double>::max();

        // 1. Find the package with the worst (lowest) benefit-to-size ratio.
        for (const Package* package : packagesInBag) {
            int packageDependenciesSize = package->getDependenciesSize();
            double ratio;

            // Handle the edge case where dependencies might have a size of 0.
            // A package with benefit and no size is infinitely valuable, so it should be the last to be removed.
            if (packageDependenciesSize <= 0) {
                // If benefit is also 0 or less, it's a prime candidate for removal.
                ratio = (package->getBenefit() <= 0) ? -1.0 : std::numeric_limits<double>::max();
            } else {
                ratio = static_cast<double>(package->getBenefit()) / packageDependenciesSize;
            }

            // If we found a new "worst" package, track it.
            if (ratio < minRatio) {
                minRatio = ratio;
                worstPackageToRemove = package;
            }
        }

        // 2. If we found a package to remove, remove it.
        if (worstPackageToRemove) {
            bag.removePackage(*worstPackageToRemove);
        } else {
            // If no package was found (e.g., all have infinite ratio), we cannot proceed.
            return false;
        }
    }

    // The bag is now within the size limit.
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
        // Handle division by zero for packages with no dependencies.
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
        // Return min if the range is invalid to avoid crashing.
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