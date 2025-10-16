#include "searchStrategies.h"
#include <chrono>

SearchStrategies::SearchStrategies(double maxTime, std::mt19937 &generator, MetaheuristicHelper &helper, std::unordered_map<const Package *, std::vector<const Dependency *>> &depGraph, const std::string &timestamp)
    : m_maxTime(maxTime), m_generator(generator), m_metaheuristicHelper(helper), m_dependencyGraph(depGraph), m_timestamp(timestamp)
{
}

Bag *SearchStrategies::randomBag(int bagSize, const std::vector<Package *> &packages)
{
    auto mutablePackages = packages;
    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickRandomPackage(pkgs);
    };
    return fillBagWithStrategy(bagSize, mutablePackages, pickStrategy, Algorithm::ALGORITHM_TYPE::RANDOM);
}

std::vector<Bag *> SearchStrategies::greedyBag(int bagSize, const std::vector<Package *> &packages)
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

std::vector<Bag *> SearchStrategies::randomGreedy(int bagSize, const std::vector<Package *> &packages)
{
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

Package* SearchStrategies::pickRandomPackage(std::vector<Package*>& packageList) {
    if (packageList.empty()) {
        return nullptr;
    }
    int index = m_metaheuristicHelper.randomNumberInt(0, packageList.size() - 1);
    Package* pickedPackage = packageList[index];
    packageList.erase(packageList.begin() + index);
    return pickedPackage;
}

Package* SearchStrategies::pickTopPackage(std::vector<Package*>& packageList) {
    if (packageList.empty()) {
        return nullptr;
    }
    Package* pickedPackage = packageList.front();
    packageList.erase(packageList.begin());
    return pickedPackage;
}

Package* SearchStrategies::pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize) {
    if (packageList.empty()) {
        return nullptr;
    }
    int candidatePoolSize = std::min(poolSize, static_cast<int>(packageList.size()));
    int index = m_metaheuristicHelper.randomNumberInt(0, candidatePoolSize - 1);
    Package* pickedPackage = packageList[index];
    packageList.erase(packageList.begin() + index);
    return pickedPackage;
}

std::vector<Package *> SearchStrategies::sortedPackagesByBenefit(const std::vector<Package *> &packages)
{
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        return a->getBenefit() > b->getBenefit();
    });
    return sortedList;
}

std::vector<Package*> SearchStrategies::sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages) {
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        double ratio_a = (a->getDependenciesSize() > 0) ? static_cast<double>(a->getBenefit()) / a->getDependenciesSize() : a->getBenefit();
        double ratio_b = (b->getDependenciesSize() > 0) ? static_cast<double>(b->getBenefit()) / b->getDependenciesSize() : b->getBenefit();
        return ratio_a > ratio_b;
    });
    return sortedList;
}

std::vector<Package*> SearchStrategies::sortedPackagesBySize(const std::vector<Package*>& packages) {
    auto sortedList = packages;
    std::sort(sortedList.begin(), sortedList.end(), [](const Package* a, const Package* b) {
        return a->getDependenciesSize() < b->getDependenciesSize();
    });
    return sortedList;
}

Bag* SearchStrategies::fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
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
    m_metaheuristicHelper.makeItFeasible(*bag, bagSize, m_dependencyGraph);

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    
    bag->setAlgorithmTime(elapsed_seconds.count());
    return bag;
}

bool SearchStrategies::canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
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


