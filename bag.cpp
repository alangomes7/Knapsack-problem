#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <string>
#include <algorithm>

Bag::Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp)
    : m_bagAlgorithm(bagAlgorithm),
      m_timeStamp(timestamp),
      m_size(0),
      m_benefit(0), // Init cached benefit
      m_algorithmTimeSeconds(0.0),
      m_localSearch(Algorithm::LOCAL_SEARCH::NONE) {
}

Bag::Bag(const std::vector<Package*>& packages)
    : m_bagAlgorithm(Algorithm::ALGORITHM_TYPE::NONE),
      m_size(0),
      m_benefit(0), // Init cached benefit
      m_algorithmTimeSeconds(0.0),
      m_localSearch(Algorithm::LOCAL_SEARCH::NONE) {
    for (const auto* pkg : packages) {
        if (pkg) {
            this->addPackage(*pkg);
        }
    }
}

const std::unordered_set<const Package*>& Bag::getPackages() const {
    return m_baggedPackages;
}

const std::unordered_set<const Dependency*>& Bag::getDependencies() const {
    return m_baggedDependencies;
}

int Bag::getSize() const {
    return m_size;
}

int Bag::getBenefit() const {
    // OPTIMIZATION: Return the cached value in O(1) time.
    return m_benefit;
}

Algorithm::ALGORITHM_TYPE Bag::getBagAlgorithm() const {
    return m_bagAlgorithm;
}

Algorithm::LOCAL_SEARCH Bag::getBagLocalSearch() const {
    return m_localSearch;
}

double Bag::getAlgorithmTime() const {
    return m_algorithmTimeSeconds;
}

std::string Bag::getTimestamp() const {
    return m_timeStamp;
}

const std::string &Bag::getMetaheuristicParameters() const
{
    return m_metaheuristicParams;
}

void Bag::setAlgorithmTime(double seconds) {
    m_algorithmTimeSeconds = seconds;
}

void Bag::setLocalSearch(Algorithm::LOCAL_SEARCH localSearch) {
    m_localSearch = localSearch;
}

void Bag::setBagAlgorithm(Algorithm::ALGORITHM_TYPE bagAlgorithm) {
    m_bagAlgorithm = bagAlgorithm;
}

void Bag::setMetaheuristicParameters(const std::string &params)
{
    m_metaheuristicParams = params;

}

bool Bag::addPackage(const Package& package) {
    auto result = m_baggedPackages.insert(&package);
    if (!result.second) {
        return false; // Package was already in the bag.
    }

    // OPTIMIZATION: Update benefit cache.
    m_benefit += package.getBenefit();

    // OPTIMIZATION: Use reference counting for dependencies.
    for (const auto& pair : package.getDependencies()) {
        const Dependency* dep = pair.second;
        m_dependencyRefCount[dep]++;
        if (m_dependencyRefCount[dep] == 1) {
            // This is a new dependency for the bag.
            m_size += dep->getSize();
            m_baggedDependencies.insert(dep);
        }
    }
    return true;
}

void Bag::removePackage(const Package& package) {
    if (m_baggedPackages.erase(&package) == 0) {
        return; // Package was not in the bag.
    }

    // OPTIMIZATION: Update benefit cache.
    m_benefit -= package.getBenefit();

    // OPTIMIZATION: Decrement reference counts and update size if a dependency is no longer needed.
    for (const auto& pair : package.getDependencies()) {
        const Dependency* dep = pair.second;
        m_dependencyRefCount[dep]--;
        if (m_dependencyRefCount[dep] == 0) {
            // This dependency is no longer required by any package.
            m_size -= dep->getSize();
            m_dependencyRefCount.erase(dep);
            m_baggedDependencies.erase(dep);
        }
    }
}

bool Bag::canAddPackage(const Package& package, int maxCapacity) const {
    int potentialSizeIncrease = 0;
    // OPTIMIZATION: Calculate size increase by checking against the ref count map.
    for (const auto& pair : package.getDependencies()) {
        const Dependency* dependency = pair.second;
        if (m_dependencyRefCount.find(dependency) == m_dependencyRefCount.end()) {
            // If the dependency is not in our map, it's a new dependency.
            potentialSizeIncrease += dependency->getSize();
        }
    }
    return (m_size + potentialSizeIncrease) <= maxCapacity;
}

bool Bag::canSwap(const Package& packageIn, const Package& packageOut, int bagSize) const {
    // OPTIMIZATION: Fast, accurate check using reference counting.
    int sizeChange = 0;

    // Calculate size increase from the incoming package's dependencies.
    for (const auto& pair : packageOut.getDependencies()) {
        const Dependency* dep = pair.second;
        // If the dependency is not already in the bag, its full size is added.
        if (m_dependencyRefCount.find(dep) == m_dependencyRefCount.end()) {
            sizeChange += dep->getSize();
        }
    }

    // Calculate size decrease from the outgoing package's dependencies.
    for (const auto& pair : packageIn.getDependencies()) {
        const Dependency* dep = pair.second;
        // If the dependency is only required by the package we are swapping out,
        // its full size will be removed.
        if (m_dependencyRefCount.at(dep) == 1) {
            sizeChange -= dep->getSize();
        }
    }

    return (m_size + sizeChange) <= bagSize;
}

std::string Bag::toString() const {
    Algorithm algoHelper(0);
    std::string bagString;

    bagString += "Algorithm: " + algoHelper.toString(m_bagAlgorithm) + " | " + algoHelper.toString(m_localSearch) + "\n";
    bagString += "Bag size: " + std::to_string(m_size) + "\n";
    bagString += "Total Benefit: " + std::to_string(m_benefit) + "\n"; // Added benefit
    bagString += "Execution Time: " + std::to_string(m_algorithmTimeSeconds) + "s\n";
    bagString += "Packages: " + std::to_string(m_baggedPackages.size()) + "\n - - - \n";

    for (const Package* package : m_baggedPackages) {
        if (package) {
            bagString += package->toString() + "\n";
        }
    }
    return bagString;
}