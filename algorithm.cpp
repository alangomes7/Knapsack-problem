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

Bag *Algorithm::run(ALGORITHM_TYPE algorithm, int bagSize,
                    const std::vector<Package *> &packages,
                    const std::vector<Dependency *> &dependencies)
{

    Bag* resultBag = nullptr;

    switch (algorithm) {
        case ALGORITHM_TYPE::RANDOM:
            resultBag = randomBag(bagSize, packages, dependencies);
            break;
        case ALGORITHM_TYPE::GREEDY:
            resultBag = greedyBagByBenefitToSizeRatio(bagSize, packages, dependencies);
            break;
        case ALGORITHM_TYPE::RANDOM_GREEDY:
            resultBag = randomGreedy(bagSize, packages, dependencies);
            break;
        case ALGORITHM_TYPE::NONE:
        default:
            // If no valid algorithm is specified, create an empty bag.
            resultBag = new Bag(ALGORITHM_TYPE::NONE);
            break;
    }
    return resultBag;
}

std::string Algorithm::toString(ALGORITHM_TYPE algorithm) const {
    // This helper function converts the algorithm enum to a human-readable string.
    switch (algorithm) {
        case ALGORITHM_TYPE::RANDOM:
            return "RANDOM";
        case ALGORITHM_TYPE::GREEDY:
            return "GREEDY";
        case ALGORITHM_TYPE::RANDOM_GREEDY:
            return "RANDOM_GREEDY";
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

    // Define the package picking strategy: select a random package from the list.
    auto pickRandomPackage = [&]() -> Package* {
        if (mutablePackages.empty()) {
            return nullptr;
        }

        int index = randomNumberInt(0, mutablePackages.size() - 1);
        Package* pickedPackage = mutablePackages[index];

        // Remove the chosen package from the list to prevent picking it again.
        mutablePackages.erase(mutablePackages.begin() + index);

        return pickedPackage;
    };

    return fillBagWithStrategy(bagSize, mutablePackages, pickRandomPackage, ALGORITHM_TYPE::RANDOM);
}

Bag* Algorithm::greedyBagByBenefitToSizeRatio(int bagSize, const std::vector<Package*>& packages,
                                         const std::vector<Dependency*>& dependencies) {
    auto sortedPackages = sortedPackagesByBenefitToSizeRatio(packages);

    // The picking strategy is to always take the next best package from the sorted list.
    auto pickTopPackage = [&sortedPackages]() -> Package* {
        if (sortedPackages.empty()) {
            return nullptr;
        }
        // Get the package at the front of the list (which has the highest benefit).
        Package* pickedPackage = sortedPackages.front();
        sortedPackages.erase(sortedPackages.begin());

        return pickedPackage;
    };

    return fillBagWithStrategy(bagSize, sortedPackages, pickTopPackage, ALGORITHM_TYPE::GREEDY);
}

Bag* Algorithm::randomGreedy(int bagSize, const std::vector<Package*>& packages,
                             const std::vector<Dependency*>& dependencies) {
    auto sortedPackages = sortedPackagesByBenefitToSizeRatio(packages);

    // The picking strategy selects randomly from the top N best packages.
    auto pickSemiRandomPackage = [&sortedPackages, this]() -> Package* {
        if (sortedPackages.empty()) {
            return nullptr;
        }
        // Define the size of the candidate pool (e.g., top 10 packages or fewer if list is small).
        int candidatePoolSize = std::min(10, static_cast<int>(sortedPackages.size()));

        // Pick a random package from this elite pool.
        int index = randomNumberInt(0, candidatePoolSize - 1);
        Package* pickedPackage = sortedPackages[index];

        sortedPackages.erase(sortedPackages.begin() + index);

        return pickedPackage;
    };

    return fillBagWithStrategy(bagSize, sortedPackages, pickSemiRandomPackage, ALGORITHM_TYPE::RANDOM_GREEDY);
}


// ===================================================================
// == Core Logic and Utilities
// ===================================================================

Bag* Algorithm::fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                                    const std::function<Package*()>& pickPackage, ALGORITHM_TYPE type) {
    // This is the core engine that iteratively fills a bag using a given method (`pickPackage`).
    auto bag = new Bag(type);
    if (packages.empty()) {
        return bag; // Return an empty bag if there are no packages to choose from.
    }

    auto compatibilityCache = std::unordered_map<const Package*, bool>();
    std::chrono::duration<double> duration = std::chrono::duration<double>(0.0);
    auto start_time = std::chrono::high_resolution_clock::now();

    // Loop until we either fill the bag
    std::chrono::duration<double> m_maxTime_seconds(m_maxTime);
    while (duration <= m_maxTime_seconds) {
        // 1. Get the next package using the provided strategy (random, greedy, etc.).
        Package* packageToAdd = pickPackage();
        if (!packageToAdd) {
            break; // Stop if the strategy provides no more packages.
        }

        // 2. Check if this package can be added without violating constraints.
        if (canPackageBeAdded(*bag, *packageToAdd, bagSize, compatibilityCache)) {
            // 3. Attempt to add the package. The Bag class handles dependency checks.
            bag->addPackage(*packageToAdd);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        duration = end_time - start_time;
    }
    bag->setAlgorithmTime(duration.count()); // Record how long the algorithm took.

    return bag;
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