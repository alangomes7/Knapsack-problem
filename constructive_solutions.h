#ifndef CONSTRUCTIVE_SOLUTIONS_H
#define CONSTRUCTIVE_SOLUTIONS_H

#include <vector>
#include <functional>
#include <random>
#include <unordered_map>
#include <memory>

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

class ConstructiveSolutions {
public:
    explicit ConstructiveSolutions(double maxTime, std::mt19937& generator,
                                 std::unordered_map<const Package*, std::vector<const Dependency*>>& depGraph,
                                 const std::string& timestamp);

    std::unique_ptr<Bag> randomBag(int bagSize, const std::vector<Package*>& packages);
    std::vector<std::unique_ptr<Bag>> greedyBag(int bagSize, const std::vector<Package*>& packages);
    std::vector<std::unique_ptr<Bag>> randomGreedy(int bagSize, const std::vector<Package*>& packages);

private:
    Package* pickRandomPackage(std::vector<Package*>& packageList);
    Package* pickTopPackage(std::vector<Package*>& packageList);
    Package* pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize = 10);

    // Return std::unique_ptr
    std::unique_ptr<Bag> fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                                 std::function<Package*(std::vector<Package*>&)> pickStrategy,
                                 ALGORITHM::ALGORITHM_TYPE type);

    std::vector<Package*> sortedPackagesByBenefit(const std::vector<Package*>& packages);
    std::vector<Package*> sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages);
    std::vector<Package*> sortedPackagesBySize(const std::vector<Package*>& packages);

    bool canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                           std::unordered_map<const Package*, bool>& cache);

    double m_maxTime;
    std::mt19937& m_generator;
    std::unordered_map<const Package*, std::vector<const Dependency*>>& m_dependencyGraph;
    std::string m_timestamp;
};

#endif // CONSTRUCTIVE_SOLUTIONS_H