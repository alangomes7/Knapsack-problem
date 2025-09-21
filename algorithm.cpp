#include "algorithm.h"

#include <random>
#include <chrono>
#include <utility>
#include <algorithm>

#include "bag.h"
#include "package.h"
#include "dependency.h"

Algorithm::Algorithm(double maxTime)
    : m_maxTime(maxTime) {
}

// Executes all strattegyies
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
    for(Bag* bagGreedy : bagsGreedy){
        resultBag.push_back(bagGreedy);
    }
    for(Bag* bagRandomGreedy : bagsRandomGreedy){
        resultBag.push_back(bagRandomGreedy);
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
        default:
            return "NONE";
    }
}

// ===================================================================
// == Private Algorithm Implementations
// ===================================================================

Bag* Algorithm::randomBag(int bagSize, const std::vector<Package*>& packages,
                          const std::vector<Dependency*>& dependencies) {
    // To avoid modifying the original list of packages, we create a mutable copy.
    auto mutablePackages = packages;

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickRandomPackage(pkgs);
    };

    return fillBagWithStrategy(bagSize, mutablePackages, pickStrategy, ALGORITHM_TYPE::RANDOM);
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

Package *Algorithm::pickRandomPackage(std::vector<Package *> &packageList)
{
    // Return nullptr immediately if the vector is empty.
    if (packageList.empty()) {
        return nullptr;
    }

    // Generate a random index within the valid range of the vector.
    int index = randomNumberInt(0, packageList.size() - 1);
    Package* pickedPackage = packageList[index];

    // Erase the chosen package from the vector using its iterator.
    packageList.erase(packageList.begin() + index);

    return pickedPackage;
}

Package *Algorithm::pickTopPackage(std::vector<Package *> &packageList)
{
    if (packageList.empty()) {
        return nullptr;
    }
    
    // Get the package at the front of the list.
    Package* pickedPackage = packageList.front();
    
    // Remove the package from the list.
    packageList.erase(packageList.begin());

    return pickedPackage;
}

Package *Algorithm::pickSemiRandomPackage(std::vector<Package *> &packageList, int poolSize)
{
    // Immediately return nullptr if the vector is empty.
    if (packageList.empty()) {
        return nullptr;
    }

    // Determine the actual size of the candidate pool. It's either the desired
    // poolSize or the total number of packages, whichever is smaller.
    int candidatePoolSize = std::min(poolSize, static_cast<int>(packageList.size()));

    // Pick a random index from within this elite candidate pool.
    int index = randomNumberInt(0, candidatePoolSize - 1);
    Package* pickedPackage = packageList[index];

    // Remove the chosen package from the vector to prevent it from being picked again.
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
    while (std::chrono::high_resolution_clock::now() - start_time < max_duration_seconds) {
        Package* packageToAdd = pickStrategy(packages);
        if (!packageToAdd) {
            break; // Stop if the strategy provides no more packages.
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

std::vector<Package *> Algorithm::sortedPackagesByBenefit(const std::vector<Package *> &packages)
{
        // Create a copy of the package list to avoid modifying the original.
    auto sortedList = packages;

    // Sort the list in-place.
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        // The comparison function sorts packages from highest benefit to lowest.
        return a->getBenefit() > b->getBenefit();
    });
    return sortedList;
}

std::vector<Package*> Algorithm::sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages) {
    // Create a copy of the package list to avoid modifying the original.
    auto sortedList = packages;

    // Sort the list in-place.
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        int aDependenciesSize = a->getDependenciesSize();
        int bDependenciesSize = b->getDependenciesSize();
        // The comparison function sorts packages from highest benefit to lowest.
        return a->getBenefit() + aDependenciesSize> b->getBenefit() + bDependenciesSize;
    });
    return sortedList;
}

std::vector<Package *> Algorithm::sortedPackagesBySize(const std::vector<Package *> &packages)
{
        // Create a copy of the package list to avoid modifying the original.
    auto sortedList = packages;

    // Sort the list in-place.
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        // The comparison function sorts packages from highest benefit to lowest.
        return a->getDependenciesSize() < b->getDependenciesSize();
    });
    return sortedList;
}

int Algorithm::randomNumberInt(int min, int max)
{
    // This function provides high-quality random numbers.
    // The generator is `static` so it's seeded only once, improving performance.
    static std::mt19937 generator(std::chrono::system_clock::now().time_since_epoch().count());

    if(min > 0) min = 0;
    
    // Ensure min is not greater than max to avoid undefined behavior.
    if (min > max) {
        std::swap(min, max);
    }

    // Create a distribution that will produce integers uniformly in the [min, max] range.
    std::uniform_int_distribution<> distribution(min, max);
    
    // Generate and return a random number.
    int randomNumber = distribution(generator);
    return randomNumber;
}

void Algorithm::precomputeDependencyGraph(const std::vector<Package *> &packages,
                                          const std::vector<Dependency *> &dependencies)
{
    // This is a stub for a potential optimization.
    // A real implementation would build a graph (e.g., an adjacency list)
    // to make dependency lookups much faster than iterating through lists every time.
}

bool Algorithm::canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                                  std::unordered_map<const Package*, bool>& cache) {
    // This function checks if a package is worth considering, using a cache (memoization) to speed up repeated checks.
    
    // 1. Check the cache first. If we've seen this package before, return the stored result.
    auto it = cache.find(&package);
    if (it != cache.end()) {
        return it->second;
    }

    // 2. If not in cache, perform the actual check using the Bag's internal logic.
    // The Bag class is responsible for the complex rules of what can be added.
    bool result = bag.canAddPackage(package, maxCapacity);

    // 3. Store the result in the cache for future lookups before returning.
    cache[&package] = result;
    
    return result;
}