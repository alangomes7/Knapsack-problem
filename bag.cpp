#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <string>
#include <algorithm>
#include <unordered_set>

Bag::Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp)
    : m_bagAlgorithm(bagAlgorithm),
      m_timeStamp(timestamp),
      m_size(0),
      m_algorithmTimeSeconds(0.0),
      m_localSearch(Algorithm::LOCAL_SEARCH::NONE) {
    // This constructor creates a new, empty bag, ready to be filled by an algorithm.
    // The size is initialized to zero.
}

Bag::Bag(const std::vector<Package*>& packages)
    : m_bagAlgorithm(Algorithm::ALGORITHM_TYPE::NONE),
      m_size(0),
      m_algorithmTimeSeconds(0.0) {
    // This constructor creates a bag from a pre-existing list of packages.
    // It iterates through the provided packages and adds each one to this bag.
    for (const auto* pkg : packages) {
        if (pkg) { // Ensure the pointer is not null
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
    // The size of the bag is the sum of the sizes of its unique dependencies.
    return m_size;
}

int Bag::getBenefit() const {
    int benefit = 0;
    for (const Package* package : m_baggedPackages) {
        benefit += package->getBenefit();
    }
    return benefit;
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

void Bag::setAlgorithmTime(double seconds) {
    m_algorithmTimeSeconds = seconds;
}

void Bag::setLocalSearch(Algorithm::LOCAL_SEARCH localSearch) {
    m_localSearch = localSearch;
}

void Bag::setBagAlgorithm(Algorithm::ALGORITHM_TYPE bagAlgorithm) {
    m_bagAlgorithm = bagAlgorithm;
}

bool Bag::addPackage(const Package& package) {
    // FIX: Use the insert() method for std::unordered_set.
    // It returns a pair, where .second is a bool that is true if insertion
    // took place (i.e., the element was not already present).
    auto result = m_baggedPackages.insert(&package);

    if (!result.second) {
        // The package was already in the bag.
        return false;
    }

    // Add its dependencies. The helper function handles duplicates and updates the bag's total size.
    addDependencies(package.getDependencies());
    return true;
}

void Bag::removePackage(const Package& package) {
    // FIX: Use erase(key) for std::unordered_set, which is more direct and efficient.
    // It returns the number of elements erased (0 or 1 for a set).
    if (m_baggedPackages.erase(&package) == 0) {
        // If the package wasn't in the bag, there's nothing to do.
        return;
    }

    // Now, we must check if any dependencies are no longer needed.
    // 1. Create a set of all dependencies that are still required by the REMAINING packages.
    std::unordered_set<const Dependency*> stillNeededDependencies;
    for (const auto* pkg : m_baggedPackages) {
        for (const auto& pair : pkg->getDependencies()) {
            stillNeededDependencies.insert(pair.second);
        }
    }

    // FIX: The remove-erase idiom is ONLY for sequence containers (like vector).
    // For an unordered_set, we must iterate and erase, or simply rebuild it.
    // Rebuilding is often cleaner and avoids iterator invalidation issues.
    m_baggedDependencies.clear();
    m_size = 0;
    for (const Dependency* dep : stillNeededDependencies) {
        m_baggedDependencies.insert(dep);
        m_size += dep->getSize();
    }
}

bool Bag::canAddPackage(const Package& package, int maxCapacity) const {
    int potentialSizeIncrease = 0;

    // Iterate through the dependencies of the package we're considering.
    for (const auto& pair : package.getDependencies()) {
        const Dependency* dependency = pair.second;

        // FIX: Use the efficient .count() method for std::unordered_set instead of std::find.
        // .count() returns 1 if the element exists, 0 otherwise.
        if (m_baggedDependencies.count(dependency) == 0) {
            // If it's not already present, adding this package would also add this dependency.
            potentialSizeIncrease += dependency->getSize();
        }
    }

    // The package can be added if the current size plus the potential increase does not exceed capacity.
    return (m_size + potentialSizeIncrease) <= maxCapacity;
}

std::string Bag::toString() const {
    // Creating a temporary instance is a simple way to access its `toString` method.
    Algorithm algoHelper(0);
    std::string bagString;

    bagString += "Algorithm: " + algoHelper.toString(m_bagAlgorithm) + " | " + algoHelper.toString(m_localSearch) + "\n";
    bagString += "Bag size: " + std::to_string(m_size) + "\n";
    bagString += "Execution Time: " + std::to_string(m_algorithmTimeSeconds) + "s\n";
    bagString += "Packages: " + std::to_string(m_baggedPackages.size()) + "\n - - - \n";

    for (const Package* package : m_baggedPackages) {
        if (package) {
            bagString += package->toString() + "\n";
        }
    }
    return bagString;
}

// ===================================================================
// == Private Helper Methods
// ===================================================================

void Bag::addDependencies(const std::unordered_map<std::string, Dependency*>& dependencies) {
    // This helper iterates through the dependencies of a newly added package.
    for (const auto& pair : dependencies) {
        const Dependency* dependency = pair.second;
        if (!dependency) continue; // Safety check

        // FIX: Use the insert() method. Its return value tells us if the element was new.
        auto result = m_baggedDependencies.insert(dependency);
        if (result.second) {
            // If .second is true, the dependency was new, so we add its size to the total.
            m_size += dependency->getSize();
        }
    }
}