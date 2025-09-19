#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <string>
#include <algorithm>
#include <unordered_set>

Bag::Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm)
    : m_bagAlgorithm(bagAlgorithm),
      m_size(0),
      m_algorithmTimeSeconds(0.0) {
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

const std::vector<const Package*>& Bag::getPackages() const {
    return m_baggedPackages;
}

const std::vector<const Dependency*>& Bag::getDependencies() const {
    return m_baggedDependencies;
}

int Bag::getSize() const {
    // The size of the bag is the sum of the sizes of its unique dependencies.
    return m_size;
}

Algorithm::ALGORITHM_TYPE Bag::getBagAlgorithm() const {
    return m_bagAlgorithm;
}

double Bag::getAlgorithmTime() const {
    return m_algorithmTimeSeconds;
}

void Bag::setAlgorithmTime(double seconds) {
    m_algorithmTimeSeconds = seconds;
}

bool Bag::addPackage(const Package& package) {
    // Use std::find to check if a pointer to this package already exists in our vector.
    auto it = std::find(m_baggedPackages.begin(), m_baggedPackages.end(), &package);
    if (it != m_baggedPackages.end()) {
        // The package is already in the bag, so we can't add it again.
        return false;
    }

    // Add the package pointer to our list.
    m_baggedPackages.push_back(&package);
    m_size = - package.getBenefit();

    // Add its dependencies. The helper function handles duplicates and updates the bag's total size.
    addDependencies(package.getDependencies());
    return true;
}

void Bag::removePackage(const Package& package) {
    // Find the iterator pointing to the package we want to remove.
    auto it = std::find(m_baggedPackages.begin(), m_baggedPackages.end(), &package);
    if (it == m_baggedPackages.end()) {
        // If the package isn't in the bag, there's nothing to do.
        return;
    }
    // Erase the package from the vector.
    m_baggedPackages.erase(it);

    // Now, we must check if any dependencies are no longer needed by any of the REMAINING packages.
    // 1. Create a set of all dependencies that are still required. A set provides fast lookups.
    std::unordered_set<const Dependency*> stillNeededDependencies;
    for (const auto* pkg : m_baggedPackages) {
        for (const auto& pair : pkg->getDependencies()) {
            stillNeededDependencies.insert(pair.second);
        }
    }

    // 2. Remove dependencies that are no longer in the "still needed" set.
    // We use the efficient remove-erase idiom to modify the vector.
    auto& deps = m_baggedDependencies;
    deps.erase(
        std::remove_if(deps.begin(), deps.end(),
            [&](const Dependency* dep) {
                // Check if this dependency is in our set of needed dependencies.
                if (stillNeededDependencies.find(dep) == stillNeededDependencies.end()) {
                    // It's NOT needed anymore. Decrement the bag size and mark it for removal.
                    m_size -= dep->getSize();
                    return true; // `remove_if` will move this element to the end for erasing.
                }
                return false; // This dependency is still needed, so don't remove it.
            }),
        deps.end()
    );
}

bool Bag::canAddPackage(const Package& package, int maxCapacity) const {
    // This is an efficient, predictive check that doesn't create a temporary bag.
    int potentialSizeIncrease = 0;

    // Iterate through the dependencies of the package we're considering.
    for (const auto& pair : package.getDependencies()) {
        const Dependency* dependency = pair.second;

        // Check if this dependency is ALREADY in our bag.
        auto it = std::find(m_baggedDependencies.begin(), m_baggedDependencies.end(), dependency);
        if (it == m_baggedDependencies.end()) {
            // If it's not already present, adding this package would also add this dependency,
            // thus increasing the bag's total size.
            potentialSizeIncrease += dependency->getSize();
        }
    }

    // The package can be added if the current size plus the potential increase does not exceed capacity.
    return (m_size + potentialSizeIncrease - package.getBenefit()) <= maxCapacity;
}

std::string Bag::toString() const {
    // To convert the algorithm enum to a string, we need an Algorithm instance.
    // Creating a temporary instance is a simple way to access its `toString` method.
    Algorithm algoHelper(0);
    std::string bagString;

    bagString += "Algorithm: " + algoHelper.toString(m_bagAlgorithm) + "\n";
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

        // Check if the dependency is already accounted for in our bag.
        auto it = std::find(m_baggedDependencies.begin(), m_baggedDependencies.end(), dependency);
        if (it == m_baggedDependencies.end()) {
            // If it's a new dependency, add it to our list and increase the total bag size.
            m_baggedDependencies.push_back(dependency);
            m_size += dependency->getSize();
        }
    }
}