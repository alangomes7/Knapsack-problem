#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <string>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <climits>

// =====================================================================================
// Constructors
// =====================================================================================

Bag::Bag(ALGORITHM::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp)
    : m_bagAlgorithm(bagAlgorithm),
      m_timeStamp(timestamp),
      m_size(0),
      m_benefit(0),
      m_algorithmTimeSeconds(0.0),
      m_localSearch(ALGORITHM::LOCAL_SEARCH::NONE) {}

Bag::Bag(const std::vector<Package*>& packages,
         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
    : m_bagAlgorithm(ALGORITHM::ALGORITHM_TYPE::NONE),
      m_size(0),
      m_benefit(0),
      m_algorithmTimeSeconds(0.0),
      m_localSearch(ALGORITHM::LOCAL_SEARCH::NONE)
{
    for (const Package* pkg : packages) {
        if (!pkg) continue;
        auto it = dependencyGraph.find(pkg);
        if (it != dependencyGraph.end())
            addPackageIfPossible(*pkg, INT_MAX, it->second);
    }
}

// =====================================================================================
// Getters
// =====================================================================================

const std::unordered_set<const Package*>& Bag::getPackages() const { return m_baggedPackages; }
const std::unordered_set<const Dependency*>& Bag::getDependencies() const { return m_baggedDependencies; }
const std::unordered_map<const Dependency*, int>& Bag::getDependencyRefCount() const { return m_dependencyRefCount; }
int Bag::getSize() const { return m_size; }
int Bag::getBenefit() const { return m_benefit; }
ALGORITHM::ALGORITHM_TYPE Bag::getBagAlgorithm() const { return m_bagAlgorithm; }
ALGORITHM::LOCAL_SEARCH Bag::getBagLocalSearch() const { return m_localSearch; }
SEARCH_ENGINE::MovementType Bag::getMovementType() const { return m_movementType; }
double Bag::getAlgorithmTime() const { return m_algorithmTimeSeconds; }
std::string Bag::getTimestamp() const { return m_timeStamp; }
std::string Bag::getMetaheuristicParameters() const { return m_metaheuristicParams; }
SOLUTION_REPAIR::FEASIBILITY_STRATEGY Bag::getFeasibilityStrategy() const { return m_feasibilityStrategy; }

std::string Bag::getAlgorithmTimeString() const {
    double total_seconds = m_algorithmTimeSeconds;
    int hours = static_cast<int>(total_seconds / 3600);
    total_seconds -= hours * 3600;
    int minutes = static_cast<int>(total_seconds / 60);
    total_seconds -= minutes * 60;
    int seconds = static_cast<int>(total_seconds);
    int ms5 = static_cast<int>((total_seconds - seconds) * 100000 + 0.5);

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << ":"
        << std::setw(5) << ms5;
    return oss.str();
}

// =====================================================================================
// Setters
// =====================================================================================

void Bag::setTimestamp(const std::string& timestamp) { m_timeStamp = timestamp; }
void Bag::setAlgorithmTime(double seconds) { m_algorithmTimeSeconds = seconds; }
void Bag::setLocalSearch(ALGORITHM::LOCAL_SEARCH localSearch) { m_localSearch = localSearch; }
void Bag::setBagAlgorithm(ALGORITHM::ALGORITHM_TYPE bagAlgorithm) { m_bagAlgorithm = bagAlgorithm; }
void Bag::setMovementType(SEARCH_ENGINE::MovementType movementType) { m_movementType = movementType; }
void Bag::setMetaheuristicParameters(const std::string& params) { m_metaheuristicParams = params; }
void Bag::setFeasibilityStrategy(SOLUTION_REPAIR::FEASIBILITY_STRATEGY feasibilityStrategy) { m_feasibilityStrategy = feasibilityStrategy; }

// =====================================================================================
// SMART BAG OPERATIONS
// =====================================================================================

bool Bag::addPackageIfPossible(const Package& package, int maxCapacity,
                              const std::vector<const Dependency*>& dependencies)
{
    if (m_baggedPackages.count(&package))
        return false;

    int addedSize = 0;
    for (const auto* dep : dependencies)
        if (!m_dependencyRefCount.count(dep))
            addedSize += dep->getSize();

    if (m_size + addedSize > maxCapacity)
        return false;

    m_baggedPackages.insert(&package);
    m_benefit += package.getBenefit();

    for (const auto* dep : dependencies) {
        auto& ref = m_dependencyRefCount[dep];
        if (ref++ == 0) {
            m_baggedDependencies.insert(dep);
            m_size += dep->getSize();
        }
    }
    return true;
}

void Bag::removePackage(const Package& package, const std::vector<const Dependency*>& dependencies)
{
    if (m_baggedPackages.erase(&package) == 0)
        return;

    m_benefit -= package.getBenefit();

    for (const auto* dep : dependencies) {
        auto it = m_dependencyRefCount.find(dep);
        if (it == m_dependencyRefCount.end()) continue;

        if (--it->second == 0) {
            m_size -= dep->getSize();
            m_baggedDependencies.erase(dep);
            m_dependencyRefCount.erase(it);
        }
    }
}

bool Bag::canSwapReadOnly(
    const std::vector<const Package*>& packagesIn,
    const std::vector<const Package*>& packagesOut,
    int bagSize,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph
) const noexcept
{
    std::unordered_map<const Dependency*, int> tempRefCount = m_dependencyRefCount;
    int sizeChange = 0;

    for (const auto* pkgIn : packagesIn) {
        auto it = dependencyGraph.find(pkgIn);
        if (it == dependencyGraph.end()) continue;
        for (const auto* dep : it->second) {
            if (tempRefCount.count(dep)) {
                tempRefCount[dep]--;
                if (tempRefCount[dep] == 0)
                    sizeChange -= dep->getSize();
            }
        }
    }

    std::unordered_set<const Dependency*> depsToAdd;
    for (const auto* pkgOut : packagesOut) {
        auto it = dependencyGraph.find(pkgOut);
        if (it != dependencyGraph.end())
            depsToAdd.insert(it->second.begin(), it->second.end());
    }

    for (const auto* dep : depsToAdd)
        if (!tempRefCount.count(dep) || tempRefCount[dep] == 0)
            sizeChange += dep->getSize();

    return (m_size + sizeChange) <= bagSize;
}

std::vector<const Package*> Bag::getInvalidPackages(
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const
{
    std::vector<const Package*> invalid;
    for (const auto* pkg : m_baggedPackages) {
        auto it = dependencyGraph.find(pkg);
        if (it == dependencyGraph.end()) {
            invalid.push_back(pkg);
            continue;
        }
        const auto& deps = it->second;
        bool valid = std::all_of(deps.begin(), deps.end(),
                                 [&](const Dependency* d){ return m_baggedDependencies.count(d) > 0; });
        if (!valid)
            invalid.push_back(pkg);
    }
    return invalid;
}

// =====================================================================================
// Utility
// =====================================================================================

std::string Bag::toString() const {
    std::string s;
    s += "Algorithm: " + ALGORITHM::toString(m_bagAlgorithm) + " | " +
         ALGORITHM::toString(m_localSearch) + "\n";
    s += "Bag size: " + std::to_string(m_size) + "\n";
    s += "Total Benefit: " + std::to_string(m_benefit) + "\n";
    s += "Execution Time: " + std::to_string(m_algorithmTimeSeconds) + "s\n";
    s += "Packages: " + std::to_string(m_baggedPackages.size()) + "\n - - - \n";
    for (const Package* p : m_baggedPackages)
        if (p) s += p->toString() + "\n";
    return s;
}
