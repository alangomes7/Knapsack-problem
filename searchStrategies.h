#ifndef SEARCHSTRATEGIES_H
#define SEARCHSTRATEGIES_H

#include <vector>
#include <functional>
#include <random>
#include <unordered_map>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "metaheuristicHelper.h"
#include "algorithm.h"

class SearchStrategies {
public:
    explicit SearchStrategies(double maxTime, std::mt19937& generator, MetaheuristicHelper& helper,
                              std::unordered_map<const Package*, std::vector<const Dependency*>>& depGraph,
                              const std::string& timestamp);

    Bag* randomBag(int bagSize, const std::vector<Package*>& packages);
    std::vector<Bag*> greedyBag(int bagSize, const std::vector<Package*>& packages);
    std::vector<Bag*> randomGreedy(int bagSize, const std::vector<Package*>& packages);

private:
    Package* pickRandomPackage(std::vector<Package*>& packageList);
    Package* pickTopPackage(std::vector<Package*>& packageList);
    Package* pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize = 10);

    Bag* fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                             std::function<Package*(std::vector<Package*>&)> pickStrategy,
                             Algorithm::ALGORITHM_TYPE type);

    std::vector<Package*> sortedPackagesByBenefit(const std::vector<Package*>& packages);
    std::vector<Package*> sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages);
    std::vector<Package*> sortedPackagesBySize(const std::vector<Package*>& packages);

    bool canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                           std::unordered_map<const Package*, bool>& cache);

    double m_maxTime;
    std::mt19937& m_generator;
    MetaheuristicHelper& m_metaheuristicHelper;
    std::unordered_map<const Package*, std::vector<const Dependency*>>& m_dependencyGraph;
    std::string m_timestamp;
};

#endif // SEARCHSTRATEGIES_H
