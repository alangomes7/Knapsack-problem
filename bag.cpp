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

Bag::Bag(const std::vector<Package*>& packages,
         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
    : m_bagAlgorithm(Algorithm::ALGORITHM_TYPE::NONE),
      m_size(0),
      m_benefit(0),
      m_algorithmTimeSeconds(0.0),
      m_localSearch(Algorithm::LOCAL_SEARCH::NONE) {

    // Iterate through the list of packages to add
    for (const Package* pkg : packages) {
        if (pkg) {
            // Find the package's dependencies in the precomputed graph (fast lookup)
            auto it = dependencyGraph.find(pkg);

            // Ensure the package exists in the graph to prevent errors
            if (it != dependencyGraph.end()) {
                // Correctly call the fast addPackage method with the cached dependency vector.
                // `it->second` is the std::vector<const Dependency*>&
                addPackage(*pkg, it->second);
            }
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

bool Bag::addPackage(const Package& package, const std::vector<const Dependency*>& dependencies) {
    // Attempt to insert the package. If it's already there, `second` will be false.
    auto [_, inserted] = m_baggedPackages.insert(&package);
    if (!inserted) {
        return false; // Package was already in the bag.
    }

    m_benefit += package.getBenefit();

    for (const Dependency* dep : dependencies) {
        // OPTIMIZATION: Perform one lookup. `operator[]` finds or creates the element.
        // We then increment its reference count.
        auto& ref_count = m_dependencyRefCount[dep];
        ref_count++;

        // If the count is 1, it's a new dependency for the bag.
        if (ref_count == 1) {
            m_size += dep->getSize();
            m_baggedDependencies.insert(dep);
        }
    }
    return true;
}

void Bag::removePackage(const Package& package, const std::vector<const Dependency*>& dependencies) {
    // Only proceed if the package was actually in the bag.
    if (m_baggedPackages.erase(&package) == 0) {
        return; // Package was not found, nothing to do.
    }

    m_benefit -= package.getBenefit();

    for (const Dependency* dep : dependencies) {
        // OPTIMIZATION: Use `find` to get an iterator with a single lookup.
        auto it = m_dependencyRefCount.find(dep);
        
        // This check ensures we don't try to operate on a non-existent dependency.
        if (it != m_dependencyRefCount.end()) {
            // Decrement the reference count.
            it->second--;

            // If the count drops to zero, remove the dependency completely.
            if (it->second == 0) {
                m_size -= dep->getSize();
                // OPTIMIZATION: Erase using the iterator is faster than erasing by key.
                m_dependencyRefCount.erase(it);
                m_baggedDependencies.erase(dep);
            }
        }
    }
}

bool Bag::canAddPackage(const Package& package, int maxCapacity, const std::vector<const Dependency*>& dependencies) const {
    int potentialSizeIncrease = 0;
    for (const Dependency* dependency : dependencies) {
        // OPTIMIZATION: Using `count` or `find` is the most direct way to check for existence.
        // `count` can be more readable. It's an O(1) operation on average.
        if (m_dependencyRefCount.count(dependency) == 0) {
            potentialSizeIncrease += dependency->getSize();
        }
    }
    return (m_size + potentialSizeIncrease) <= maxCapacity;
}

bool Bag::canSwap(const Package& packageIn, const Package& packageOut, int bagSize) const {
    // This function's logic is already quite efficient, relying on single lookups
    // for dependency checks. No major changes are needed here.
    int sizeChange = 0;

    // Calculate size increase from the incoming package's dependencies.
    for (const auto& pair : packageOut.getDependencies()) {
        const Dependency* dep = pair.second;
        // If the dependency is not already in the bag, its full size is added.
        if (m_dependencyRefCount.count(dep) == 0) {
            sizeChange += dep->getSize();
        }
    }

    // Calculate size decrease from the outgoing package's dependencies.
    for (const auto& pair : packageIn.getDependencies()) {
        const Dependency* dep = pair.second;
        auto it = m_dependencyRefCount.find(dep);
        if (it != m_dependencyRefCount.end()) {
            // If the dependency is only required by the package we are swapping out,
            // its full size will be removed.
            if (it->second == 1) {
                sizeChange -= dep->getSize();
            }
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