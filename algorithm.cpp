#include "algorithm.h"

#include <chrono>
#include <utility>
#include <algorithm>
#include <iterator>
#include <limits>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "vnd.h"
#include "vns.h"
#include "grasp.h"

Algorithm::Algorithm(double maxTime)
    : m_maxTime(maxTime) {
}

Algorithm::Algorithm(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_generator(seed)
{
}

// Executes all strategies
std::vector<Bag*> Algorithm::run(Algorithm::ALGORITHM_TYPE algorithm,int bagSize,
                       const std::vector<Package *> &packages,
                       const std::vector<Dependency *> &dependencies,
                       const std::string& timestamp)
{
    m_timestamp = timestamp;
    precomputeDependencyGraph(packages, dependencies);
    std::vector<Bag*> resultBag;
    resultBag.reserve(11);
    
    resultBag.push_back(randomBag(bagSize, packages, dependencies));
    std::vector<Bag*> bagsGreedy = greedyBag(bagSize, packages, dependencies);
    std::vector<Bag*> bagsRandomGreedy = randomGreedy(bagSize, packages, dependencies);

    resultBag.insert(resultBag.end(), bagsGreedy.begin(), bagsGreedy.end());
    resultBag.insert(resultBag.end(), bagsRandomGreedy.begin(), bagsRandomGreedy.end());

    Bag* bestInitialBag = nullptr;
    int bestInitialBenefit = -1;
    for (Bag* b : resultBag) {
        if (b->getBenefit() > bestInitialBenefit) {
            bestInitialBenefit = b->getBenefit();
            bestInitialBag = b;
        }
    }

    // Run metaheuristics using the best initial solution
    // Run metaheuristics using the best initial solution
    if (bestInitialBag) {
        // Instantiate and run VND
        VND vnd(m_maxTime);
        Bag* vndBag = vnd.run(bagSize, bestInitialBag, packages, m_dependencyGraph);
        resultBag.push_back(vndBag);

        // Instantiate and run VNS for each local search method, seeding it from the main generator
        VNS vns(m_maxTime, m_generator()); 
        Bag* vnsFirstBag = vns.run(bagSize, bestInitialBag, packages, Algorithm::LOCAL_SEARCH::FIRST_IMPROVEMENT, m_dependencyGraph);
        Bag* vnsBestBag = vns.run(bagSize, bestInitialBag, packages, Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT, m_dependencyGraph);
        Bag* vnsRandomBag = vns.run(bagSize, bestInitialBag, packages, Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT, m_dependencyGraph);
        resultBag.push_back(vnsFirstBag);
        resultBag.push_back(vnsBestBag);
        resultBag.push_back(vnsRandomBag);
        
        // Instantiate and run GRASP
        GRASP grasp(m_maxTime, m_generator());
        resultBag.push_back(grasp.run(bagSize, resultBag, packages, Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT, m_dependencyGraph));
    }

    return resultBag;
}

std::string Algorithm::toString(Algorithm::ALGORITHM_TYPE algorithm) const {
    switch (algorithm) {
        case Algorithm::ALGORITHM_TYPE::RANDOM: return "RANDOM";
        case Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit: return "GREEDY -> Package: Benefit";
        case Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio: return "GREEDY -> Package: Benefit Ratio";
        case Algorithm::ALGORITHM_TYPE::GREEDY_Package_Size: return "GREEDY -> Package: Size";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit: return "RANDOM_GREEDY (Package: Benefit)";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio: return "RANDOM_GREEDY (Package: Benefit Ratio)";
        case Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size: return "RANDOM_GREEDY (Package: Size)";
        case Algorithm::ALGORITHM_TYPE::VND: return "VND";
        case Algorithm::ALGORITHM_TYPE::VNS: return "VNS";
        case Algorithm::ALGORITHM_TYPE::GRASP: return "GRASP";
        default: return "NONE";
    }
}

std::string Algorithm::toString(Algorithm::LOCAL_SEARCH localSearch) const
{
    switch (localSearch) {
        case Algorithm::LOCAL_SEARCH::FIRST_IMPROVEMENT: return "First Improvement";
        case Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT: return "Best Improvement";
        case Algorithm::LOCAL_SEARCH::RANDOM_IMPROVEMENT: return "Random Improvement";
        default: return "None";
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
    return fillBagWithStrategy(bagSize, mutablePackages, pickStrategy, Algorithm::ALGORITHM_TYPE::RANDOM);
}

std::vector<Bag *> Algorithm::greedyBag(int bagSize, const std::vector<Package *> &packages,
                                        const std::vector<Dependency *> &dependencies)
{
    std::vector<Bag*> bags;
    bags.reserve(3);
    
    std::vector<Package *> sortedByRatio = sortedPackagesByBenefitToSizeRatio(packages);
    std::vector<Package *> sortedByBenefit = sortedPackagesByBenefit(packages);
    std::vector<Package *> sortedBySize = sortedPackagesBySize(packages);

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickTopPackage(pkgs);
    };

    bags.push_back(fillBagWithStrategy(bagSize, sortedByRatio, pickStrategy, Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit_Ratio));
    bags.push_back(fillBagWithStrategy(bagSize, sortedByBenefit, pickStrategy, Algorithm::ALGORITHM_TYPE::GREEDY_Package_Benefit));
    bags.push_back(fillBagWithStrategy(bagSize, sortedBySize, pickStrategy, Algorithm::ALGORITHM_TYPE::GREEDY_Package_Size));
    return bags;
}

std::vector<Bag*> Algorithm::randomGreedy(int bagSize, const std::vector<Package*>& packages,
                                          const std::vector<Dependency*>& dependencies) {
    std::vector<Bag*> bags;
    bags.reserve(3);
    
    std::vector<Package *> sortedByRatio = sortedPackagesByBenefitToSizeRatio(packages);
    std::vector<Package *> sortedByBenefit = sortedPackagesByBenefit(packages);
    std::vector<Package *> sortedBySize = sortedPackagesBySize(packages);

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        const int candidatePoolSize = 10;
        return this->pickSemiRandomPackage(pkgs, candidatePoolSize);
    };

    bags.push_back(fillBagWithStrategy(bagSize, sortedByBenefit, pickStrategy, Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit));
    bags.push_back(fillBagWithStrategy(bagSize, sortedByRatio, pickStrategy, Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Benefit_Ratio));
    bags.push_back(fillBagWithStrategy(bagSize, sortedBySize, pickStrategy, Algorithm::ALGORITHM_TYPE::RANDOM_GREEDY_Package_Size));
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
                                      Algorithm::ALGORITHM_TYPE type) {
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
            const auto& deps = m_dependencyGraph.at(packageToAdd);
            bag->addPackage(*packageToAdd, deps);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bag->setAlgorithmTime(elapsed_seconds.count());
    return bag;
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
    if (min > max) {
        return min;
    }
    std::uniform_int_distribution<> distribution(min, max);
    return distribution(m_generator);
}

void Algorithm::precomputeDependencyGraph(const std::vector<Package*>& packages,
                                          const std::vector<Dependency*>& dependencies)
{
    m_dependencyGraph.clear();
    m_dependencyGraph.reserve(packages.size());

    for (const auto* package : packages) {
        if (package) {
            const auto& packageDependencies = package->getDependencies();
            std::vector<const Dependency*> depVector;
            depVector.reserve(packageDependencies.size());
            for (const auto& pair : packageDependencies) {
                depVector.push_back(pair.second);
            }
            m_dependencyGraph[package] = std::move(depVector);
        }
    }
}

bool Algorithm::canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                                  std::unordered_map<const Package*, bool>& cache) {
    auto it = cache.find(&package);
    if (it != cache.end()) {
        return it->second;
    }
     const auto& deps = m_dependencyGraph.at(&package);
    bool result = bag.canAddPackage(package, maxCapacity, deps);
    cache[&package] = result;
    return result;
}