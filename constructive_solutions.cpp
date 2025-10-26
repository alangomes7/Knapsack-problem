#include "constructive_solutions.h"

#include <chrono>
#include <algorithm>
#include <memory>
#include <unordered_set> // Include for unordered_set

#include "random_provider.h"
#include "solution_repair.h"

ConstructiveSolutions::ConstructiveSolutions(double maxTime, std::mt19937& generator,
                              std::unordered_map<const Package*, std::vector<const Dependency*>>& depGraph,
                              const std::string& timestamp)
    : m_maxTime(maxTime), m_generator(generator), m_dependencyGraph(depGraph), m_timestamp(timestamp)
{
}

// Return std::unique_ptr<Bag>
std::unique_ptr<Bag> ConstructiveSolutions::randomBag(int bagSize, const std::vector<Package *> &packages)
{
    auto mutablePackages = packages;
    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickRandomPackage(pkgs);
    };
    return fillBagWithStrategy(bagSize, mutablePackages, pickStrategy, ALGORITHM::ALGORITHM_TYPE::RANDOM);
}

// Return std::vector<std::unique_ptr<Bag>>
std::vector<std::unique_ptr<Bag>> ConstructiveSolutions::greedyBag(int bagSize, const std::vector<Package *> &packages)
{
    // Vector now holds unique_ptrs
    std::vector<std::unique_ptr<Bag>> bags;
    bags.reserve(3);
    
    std::vector<Package *> sortedByBenefit = sortedPackagesByBenefit(packages);
    std::vector<Package *> sortedByRatio = sortedPackagesByBenefitToSizeRatio(packages);
    std::vector<Package *> sortedBySize = sortedPackagesBySize(packages);

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickTopPackage(pkgs);
    };

    // fillBagWithStrategy returns a unique_ptr, which is moved into the vector
    bags.push_back(fillBagWithStrategy(bagSize, sortedByBenefit, pickStrategy, ALGORITHM::ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT));
    bags.push_back(fillBagWithStrategy(bagSize, sortedByRatio, pickStrategy, ALGORITHM::ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT_RATIO));
    bags.push_back(fillBagWithStrategy(bagSize, sortedBySize, pickStrategy, ALGORITHM::ALGORITHM_TYPE::GREEDY_PACKAGE_SIZE));
    return bags;
}

// Return std::vector<std::unique_ptr<Bag>>
std::vector<std::unique_ptr<Bag>> ConstructiveSolutions::randomGreedy(int bagSize, const std::vector<Package *> &packages)
{
    // Vector now holds unique_ptrs
    std::vector<std::unique_ptr<Bag>> bags;
    bags.reserve(3);
    
    std::vector<Package *> sortedByBenefit = sortedPackagesByBenefit(packages);
    std::vector<Package *> sortedByRatio = sortedPackagesByBenefitToSizeRatio(packages);
    std::vector<Package *> sortedBySize = sortedPackagesBySize(packages);

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        const int candidatePoolSize = 10;
        return this->pickSemiRandomPackage(pkgs, candidatePoolSize);
    };

    // fillBagWithStrategy returns a unique_ptr, which is moved into the vector
    bags.push_back(fillBagWithStrategy(bagSize, sortedByBenefit, pickStrategy, ALGORITHM::ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT));
    bags.push_back(fillBagWithStrategy(bagSize, sortedByRatio, pickStrategy, ALGORITHM::ALGORITHM_TYPE::GREEDY_PACKAGE_BENEFIT_RATIO));
    bags.push_back(fillBagWithStrategy(bagSize, sortedBySize, pickStrategy, ALGORITHM::ALGORITHM_TYPE::RANDOM_GREEDY_PACKAGE_SIZE));
    return bags;
}

Package* ConstructiveSolutions::pickRandomPackage(std::vector<Package*>& packageList) {
    if (packageList.empty()) {
        return nullptr;
    }
    // Note: RANDOM_PROVIDER::getInt is inclusive, so (0, size - 1) is correct
    int index = RANDOM_PROVIDER::getInt(0, packageList.size() - 1, m_generator);
    Package* pickedPackage = packageList[index];
    packageList.erase(packageList.begin() + index);
    return pickedPackage;
}

Package* ConstructiveSolutions::pickTopPackage(std::vector<Package*>& packageList) {
    if (packageList.empty()) {
        return nullptr;
    }
    Package* pickedPackage = packageList.front();
    packageList.erase(packageList.begin());
    return pickedPackage;
}

// Removed default argument 'poolSize = 10' as it's already in the header
Package* ConstructiveSolutions::pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize) {
    if (packageList.empty()) {
        return nullptr;
    }
    int candidatePoolSize = std::min(poolSize, static_cast<int>(packageList.size()));
    // Handle edge case where candidatePoolSize could be 0 if poolSize is <= 0
    if (candidatePoolSize <= 0) {
        return nullptr;
    }
    int index = RANDOM_PROVIDER::getInt(0, candidatePoolSize - 1, m_generator);
    Package* pickedPackage = packageList[index];
    packageList.erase(packageList.begin() + index);
    return pickedPackage;
}

std::vector<Package *> ConstructiveSolutions::sortedPackagesByBenefit(const std::vector<Package *> &packages)
{
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        return a->getBenefit() > b->getBenefit();
    });
    return sortedList;
}

std::vector<Package*> ConstructiveSolutions::sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages) {
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        double ratio_a = (a->getDependenciesSize() > 0) ? static_cast<double>(a->getBenefit()) / a->getDependenciesSize() : a->getBenefit();
        double ratio_b = (b->getDependenciesSize() > 0) ? static_cast<double>(b->getBenefit()) / b->getDependenciesSize() : b->getBenefit();
        return ratio_a > ratio_b;
    });
    return sortedList;
}

std::vector<Package*> ConstructiveSolutions::sortedPackagesBySize(const std::vector<Package*>& packages) {
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        return a->getDependenciesSize() < b->getDependenciesSize();
    });
    return sortedList;
}

// Return std::unique_ptr<Bag>
std::unique_ptr<Bag> ConstructiveSolutions::fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                                      std::function<Package*(std::vector<Package*>&)> pickStrategy,
                                      ALGORITHM::ALGORITHM_TYPE type) {
    // Use std::make_unique to create the Bag
    auto bag = std::make_unique<Bag>(type, m_timestamp);
    if (packages.empty()) {
        return bag;
    }

    auto compatibilityCache = std::unordered_map<const Package*, bool>();
    auto inBagCache = std::unordered_set<const Package*>(); // FIX: Create the new cache
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

        // FIX: Pass the new cache to the function
        canPackageBeAdded(*bag, *packageToAdd, bagSize, compatibilityCache, inBagCache);
    }
    
    // Pass the dereferenced unique_ptr (*bag)
    SOLUTION_REPAIR::repair(*bag, bagSize, m_dependencyGraph, m_generator());

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    
    bag->setAlgorithmTime(elapsed_seconds.count());
    return bag; // Return the std::unique_ptr
}

// FIX: Added inBagCache parameter and logic
bool ConstructiveSolutions::canPackageBeAdded(Bag& bag, const Package& package, int maxCapacity,
                                  std::unordered_map<const Package*, bool>& cache,
                                  std::unordered_set<const Package*>& inBagCache) {
    
    // FIX: Check if package is already in the bag cache
    if (inBagCache.count(&package)) {
        return false;
    }
    
    auto it = cache.find(&package);
    if (it != cache.end()) {
        return it->second;
    }
    const auto& deps = m_dependencyGraph.at(&package);
    bool result = bag.addPackageIfPossible(package, maxCapacity, deps);
    cache[&package] = result;
    
    if (result) {
        inBagCache.insert(&package);
    }
    
    return result;
}