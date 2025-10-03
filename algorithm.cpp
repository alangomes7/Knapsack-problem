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

    // Find the best greedy solution to use as a starting point for VND
    Bag* bestGreedyBag = nullptr;
    int bestGreedyBenefit = -1;
    for (Bag* b : resultBag) {
        if (b->getBenefit() > bestGreedyBenefit) {
            bestGreedyBenefit = b->getBenefit();
            bestGreedyBag = b;
        }
    }

    resultBag.push_back(vndBag(bagSize, bestGreedyBag, packages));
    resultBag.push_back(vnsBag(bagSize, bestGreedyBag, packages, LOCAL_SEARCH::FIRST_IMPROVEMENT));
    resultBag.push_back(vnsBag(bagSize, bestGreedyBag, packages, LOCAL_SEARCH::BEST_IMPROVEMENT));
    resultBag.push_back(vnsBag(bagSize, bestGreedyBag, packages, LOCAL_SEARCH::RANDOM_IMPROVEMENT));
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
    // To avoid modifying the original list of packages, we create a mutable copy.
    auto mutablePackages = packages;

    auto pickStrategy = [&](std::vector<Package*>& pkgs) {
        return this->pickRandomPackage(pkgs);
    };

    return fillBagWithStrategy(bagSize, mutablePackages, pickStrategy, ALGORITHM_TYPE::RANDOM);
}

Bag *Algorithm::vndBag(int bagSize, Bag *initialBag, const std::vector<Package *> &allPackages)
{
    auto bestBag = new Bag(*initialBag);
    bestBag->setBagAlgorithm(ALGORITHM_TYPE::VND);
    bool improved = true;

    std::chrono::duration<double> max_duration_seconds(m_maxTime);
    auto start_time = std::chrono::high_resolution_clock::now();

    while (improved && std::chrono::high_resolution_clock::now() - start_time < max_duration_seconds) {
        improved = false;
        // Neighborhood 1: Swap one package from the bag with one outside
        const auto& packagesInBag = bestBag->getPackages();
        std::vector<Package*> packagesOutsideBag;

        for (Package* p : allPackages) {
            bool inBag = false;
            for (const Package* pInBag : packagesInBag) {
                if (p->getName() == pInBag->getName()) {
                    inBag = true;
                    break;
                }
            }
            if (!inBag) {
                packagesOutsideBag.push_back(p);
            }
        }

        for (const Package* packageIn : packagesInBag) {
            for (Package* packageOut : packagesOutsideBag) {
                Bag tempBag = *bestBag;
                tempBag.removePackage(*packageIn);
                if (tempBag.canAddPackage(*packageOut, bagSize)) {
                    tempBag.addPackage(*packageOut);
                    if (tempBag.getBenefit() > bestBag->getBenefit()) {
                        delete bestBag;
                        bestBag = new Bag(tempBag);
                        improved = true;
                        goto next_iteration; // Exit inner loops and restart with the new best bag
                    }
                }
            }
        }
        next_iteration:;
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

    while (std::chrono::high_resolution_clock::now() - start_time < max_duration_seconds && k <= k_max) {
        // 1. Shaking: Generate a random solution in the k-th neighborhood
        Bag* shakenBag = shake(*bestBag, k, allPackages, bagSize);

        // 2. Local Search: Find the local optimum from the shaken solution
        localSearch(*shakenBag, bagSize, allPackages, localSearchMethod);

        // 3. Move: If the new solution is better, update the best solution
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

    // Randomly remove k packages
    for (int i = 0; i < k && !newBag->getPackages().empty(); ++i) {
        int index = randomNumberInt(0, newBag->getPackages().size() - 1);
        const Package* packageToRemove = newBag->getPackages()[index];
        newBag->removePackage(*packageToRemove);
    }

    // Randomly add k packages
    std::vector<Package*> packagesOutside;
    for (Package* p : allPackages) {
        bool inBag = false;
        for (const auto* pkgInBag : newBag->getPackages()) {
            if (p->getName() == pkgInBag->getName()) {
                inBag = true;
                break;
            }
        }
        if (!inBag) {
            packagesOutside.push_back(p);
        }
    }

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
    switch (localSearchMethod)
    {
        case LOCAL_SEARCH::BEST_IMPROVEMENT:
            while (!localOptimum) {
                if (exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages)) {
                    improved = true;
                } else {
                    localOptimum = true;
            }
        }
        break;

        case LOCAL_SEARCH::RANDOM_IMPROVEMENT:
            while (!localOptimum) {
                if (exploreSwapNeighborhoodRandomImprovement(currentBag, bagSize, allPackages)) {
                    improved = true;
                } else {
                    localOptimum = true;
            }
        }
        break;
    
        default:
            while (!localOptimum) {
                if (exploreSwapNeighborhoodFirstImprovement(currentBag, bagSize, allPackages)) {
                    improved = true;
                } else {
                    localOptimum = true;
                }
            }
    }
    return improved;
}

bool Algorithm::exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages) {
    const auto& packagesInBag = currentBag.getPackages();
    std::vector<Package*> packagesOutsideBag;
    for (Package* p : allPackages) {
        bool inBag = false;
        for (const Package* pInBag : packagesInBag) {
            if (p->getName() == pInBag->getName()) {
                inBag = true;
                break;
            }
        }
        if (!inBag) {
            packagesOutsideBag.push_back(p);
        }
    }

    for (const Package* packageIn : packagesInBag) {
        for (Package* packageOut : packagesOutsideBag) {
            Bag tempBag = currentBag;
            tempBag.removePackage(*packageIn);
            if (tempBag.canAddPackage(*packageOut, bagSize)) {
                tempBag.addPackage(*packageOut);
                if (tempBag.getBenefit() > currentBag.getBenefit()) {
                    currentBag = tempBag;
                    return true; // Improvement found
                }
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

    // Popula a lista de pacotes que não estão na mochila
    for (Package* p : allPackages) {
        bool inBag = false;
        for (const Package* pInBag : packagesInBag) {
            if (p->getName() == pInBag->getName()) {
                inBag = true;
                break;
            }
        }
        if (!inBag) {
            packagesOutsideBag.push_back(p);
        }
    }

    // Itera por todas as trocas possíveis
    for (const Package* packageIn : packagesInBag) {
        for (Package* packageOut : packagesOutsideBag) {
            Bag tempBag = currentBag;
            tempBag.removePackage(*packageIn);

            if (tempBag.canAddPackage(*packageOut, bagSize)) {
                tempBag.addPackage(*packageOut);
                // Se o benefício deste vizinho é o melhor até agora
                if (tempBag.getBenefit() > bestNeighborBag.getBenefit()) {
                    bestNeighborBag = tempBag;
                    improvementFound = true;
                }
            }
        }
    }

    // Se uma melhoria foi encontrada, atualiza a mochila atual para a melhor vizinha
    if (improvementFound) {
        currentBag = bestNeighborBag;
    }

    return improvementFound;
}

bool Algorithm::exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages) {
    std::vector<Bag> improvingNeighbors;

    const auto& packagesInBag = currentBag.getPackages();
    std::vector<Package*> packagesOutsideBag;
    // Popula a lista de pacotes que não estão na mochila
    for (Package* p : allPackages) {
        bool inBag = false;
        for (const Package* pInBag : packagesInBag) {
            if (p->getName() == pInBag->getName()) {
                inBag = true;
                break;
            }
        }
        if (!inBag) {
            packagesOutsideBag.push_back(p);
        }
    }

    // Itera por todas as trocas possíveis
    for (const Package* packageIn : packagesInBag) {
        for (Package* packageOut : packagesOutsideBag) {
            Bag tempBag = currentBag;
            tempBag.removePackage(*packageIn);

            if (tempBag.canAddPackage(*packageOut, bagSize)) {
                tempBag.addPackage(*packageOut);
                // Se a troca melhora a solução, adiciona à lista de candidatos
                if (tempBag.getBenefit() > currentBag.getBenefit()) {
                    improvingNeighbors.push_back(tempBag);
                }
            }
        }
    }

    // Se encontramos vizinhos que melhoram a solução
    if (!improvingNeighbors.empty()) {
        // Escolhe um dos vizinhos aleatoriamente
        int randomIndex = randomNumberInt(0, improvingNeighbors.size() - 1);
        currentBag = improvingNeighbors[randomIndex];
        return true;
    }

    return false; // Nenhuma melhoria encontrada
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