#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>

class Bag;
class Package;
class Dependency;

/**
 * @brief Provides a collection of algorithms to solve the package dependency knapsack problem.
 *
 * This class acts as a solver engine for a variation of the classic knapsack problem,
 * where items (Packages) have inter-dependencies. It orchestrates constructive heuristics
 * and delegates to metaheuristic classes like VND and VNS for solution improvement. 🧠⚙️
 */
class Algorithm {
public:
    enum class ALGORITHM_TYPE {
        NONE,
        RANDOM,
        GREEDY_Package_Benefit,
        GREEDY_Package_Benefit_Ratio,
        GREEDY_Package_Size,
        RANDOM_GREEDY_Package_Benefit,
        RANDOM_GREEDY_Package_Benefit_Ratio,
        RANDOM_GREEDY_Package_Size,
        VND,
        VNS
    };

    enum class LOCAL_SEARCH {
        FIRST_IMPROVEMENT,
        BEST_IMPROVEMENT,
        RANDOM_IMPROVEMENT,
        NONE
    };

    explicit Algorithm(double maxTime);
    explicit Algorithm(double maxTime, unsigned int seed);

    std::vector<Bag*> run(Algorithm::ALGORITHM_TYPE algorithm, int bagSize,
                          const std::vector<Package*>& packages,
                          const std::vector<Dependency*>& dependencies,
                          const std::string& timestamp);

    std::string toString(Algorithm::ALGORITHM_TYPE algorithm) const;
    std::string toString(Algorithm::LOCAL_SEARCH localSearch) const;

private:
    Package* pickRandomPackage(std::vector<Package*>& packageList);
    Package* pickTopPackage(std::vector<Package*>& packageList);
    Package* pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize = 10);
    Bag* fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                             std::function<Package*(std::vector<Package*>&)> pickStrategy,
                             Algorithm::ALGORITHM_TYPE type);

    Bag* randomBag(int bagSize, const std::vector<Package*>& packages,
                   const std::vector<Dependency*>& dependencies);

    std::vector<Bag*> greedyBag(int bagSize, const std::vector<Package*>& packages,
                                const std::vector<Dependency*>& dependencies);

    std::vector<Bag*> randomGreedy(int bagSize, const std::vector<Package*>& packages,
                                   const std::vector<Dependency*>& dependencies);

    std::vector<Package*> sortedPackagesByBenefit(const std::vector<Package*>& packages);
    std::vector<Package*> sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages);
    std::vector<Package*> sortedPackagesBySize(const std::vector<Package*>& packages);

    int randomNumberInt(int min, int max);

    void precomputeDependencyGraph(const std::vector<Package*>& packages,
                                   const std::vector<Dependency*>& dependencies);

    bool canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                           std::unordered_map<const Package*, bool>& cache);

    std::unordered_map<const Package*, std::vector<const Dependency*>> m_dependencyGraph;
    const double m_maxTime;
    std::string m_timestamp = "0";
    std::mt19937 m_generator;
};

#endif // ALGORITHM_H