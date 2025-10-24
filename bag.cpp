#include "bag.h"
#include "package.h"
#include "dependency.h"

#include <string>
#include <algorithm>
#include <numeric>

// =====================================================================================
// Constructors and Standard Methods
// =====================================================================================

Bag::Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp)
    : m_bagAlgorithm(bagAlgorithm), m_timeStamp(timestamp), m_size(0), m_benefit(0),
      m_algorithmTimeSeconds(0.0), m_localSearch(Algorithm::LOCAL_SEARCH::NONE) {}

Bag::Bag(const std::vector<Package*>& packages,
         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
    : m_bagAlgorithm(Algorithm::ALGORITHM_TYPE::NONE), m_size(0), m_benefit(0),
      m_algorithmTimeSeconds(0.0), m_localSearch(Algorithm::LOCAL_SEARCH::NONE) {
    for (const Package* pkg : packages) {
        if (pkg) {
            auto it = dependencyGraph.find(pkg);
            if (it != dependencyGraph.end()) {
                addPackage(*pkg, it->second);
            }
        }
    }
}
// =====================================================================================
// Getters
// =====================================================================================

const std::unordered_set<const Package*>& Bag::getPackages() const { return m_baggedPackages; }
const std::unordered_set<const Dependency*>& Bag::getDependencies() const { return m_baggedDependencies; }
const std::unordered_map<const Dependency *, int> &Bag::getDependencyRefCount() const{ return m_dependencyRefCount; }
int Bag::getSize() const { return m_size; }
int Bag::getBenefit() const { return m_benefit; }
/**
 * @brief Gets the algorithm type used to create the bag.
 * @return The algorithm type.
 */
Algorithm::ALGORITHM_TYPE Bag::getBagAlgorithm() const {
    return m_bagAlgorithm;
}

/**
 * @brief Gets the local search method applied to the bag.
 * @return The local search type.
 */
Algorithm::LOCAL_SEARCH Bag::getBagLocalSearch() const {
    return m_localSearch;
}

SearchEngine::MovementType Bag::getMovementType() const
{
    return m_movementType;
}

/**
 * @brief Gets the execution time of the algorithm.
 * @return The execution time in seconds.
 */
double Bag::getAlgorithmTime() const {
    return m_algorithmTimeSeconds;
}

std::string Bag::getAlgorithmTimeString() const
{
    double total_seconds = m_algorithmTimeSeconds;

    // Break down hours and minutes
    int hours = static_cast<int>(total_seconds / 3600);
    total_seconds -= hours * 3600;

    int minutes = static_cast<int>(total_seconds / 60);
    total_seconds -= minutes * 60;

    int seconds = static_cast<int>(total_seconds);

    // 5-digit rounded fractional seconds (hundred-thousandths)
    int ms5 = static_cast<int>((total_seconds - seconds) * 100000 + 0.5);

    // Format: HH:MM:SS:MSMSMSMSMS (5 digits)
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setw(2) << minutes << ":"
        << std::setw(2) << seconds << ":"
        << std::setw(5) << ms5;

    return oss.str();
}

/**
 * @brief Gets the timestamp for when the bag was created.
 * @return The timestamp string.
 */
std::string Bag::getTimestamp() const {
    return m_timeStamp;
}

/**
 * @brief Gets the string of parameters used by a metaheuristic algorithm.
 * @return The metaheuristic parameters.
 */
std::string Bag::getMetaheuristicParameters() const {
    return m_metaheuristicParams;
}

SolutionRepair::FeasibilityStrategy Bag::getFeasibilityStrategy() const
{
    return m_feasibilityStrategy;
}

// =====================================================================================
// Setters
// =====================================================================================

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

void Bag::setTimestamp(const std::string& timestamp) {
    m_timeStamp = timestamp;
}

/**
 * @brief Sets the algorithm execution time.
 * @param seconds The execution time in seconds.
 */
void Bag::setAlgorithmTime(double seconds) {
    m_algorithmTimeSeconds = seconds;
}

/**
 * @brief Sets the local search method used.
 * @param localSearch The local search type.
 */
void Bag::setLocalSearch(Algorithm::LOCAL_SEARCH localSearch) {
    m_localSearch = localSearch;
}

/**
 * @brief Sets the main algorithm type used.
 * @param bagAlgorithm The algorithm type.
 */
void Bag::setBagAlgorithm(Algorithm::ALGORITHM_TYPE bagAlgorithm) {
    m_bagAlgorithm = bagAlgorithm;
}

void Bag::setMovementType(SearchEngine::MovementType movementType)
{
    m_movementType = movementType;
}

/**
 * @brief Sets the metaheuristic parameter string.
 * @param params The parameter string.
 */
void Bag::setMetaheuristicParameters(const std::string& params) {
    m_metaheuristicParams = params;
}

void Bag::setFeasibilityStrategy(SolutionRepair::FeasibilityStrategy feasibilityStrategy)
{
    m_feasibilityStrategy = feasibilityStrategy;
}

// =====================================================================================
// New & Corrected Read-Only Feasibility Checks
// =====================================================================================

/**
 * @brief [BUG FIX] The original implementation was incorrect for shared dependencies.
 * This corrected version accurately calculates the size change by checking dependency reference counts.
 */
bool Bag::canSwapReadOnly(const Package& packageIn, const Package& packageOut, int bagSize) const noexcept
{
    int sizeChange = 0;

    // Calculate size reduction from removing packageIn
    for (const auto& dep : packageIn.getDependencies()) {
        // If this dependency is only used by packageIn, its size will be removed.
        if (m_dependencyRefCount.count(dep.second) && m_dependencyRefCount.at(dep.second) == 1) {
            sizeChange -= dep.second->getSize();
        }
    }

    // Calculate size increase from adding packageOut
    for (const auto& dep : packageOut.getDependencies()) {
        // If this dependency is not already in the bag, its size will be added.
        // This correctly handles the case where packageIn's removal doesn't remove a shared dependency.
        if (m_dependencyRefCount.count(dep.second) == 0) {
            sizeChange += dep.second->getSize();
        }
    }

    return (m_size + sizeChange) <= bagSize;
}

/**
 * @brief Logic for checking a 1-to-N swap.
 */
bool Bag::canSwapReadOnly(const Package& packageIn, const std::vector<Package*>& packagesOut, int bagSize,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const noexcept
{
    int sizeChange = 0;
    const auto& depsIn = dependencyGraph.at(&packageIn);

    // Calculate size reduction from removing packageIn
    for (const auto* dep : depsIn) {
        if (m_dependencyRefCount.count(dep) && m_dependencyRefCount.at(dep) == 1) {
            sizeChange -= dep->getSize();
        }
    }

    // Create a temporary set of all unique dependencies that will be added
    std::unordered_set<const Dependency*> depsOutUnique;
    for (const auto* p_out : packagesOut) {
        const auto& deps = dependencyGraph.at(p_out);
        depsOutUnique.insert(deps.begin(), deps.end());
    }

    // Calculate size increase from adding all new unique dependencies
    for (const auto* dep : depsOutUnique) {
        // If the dependency is not already present in the bag, its size contributes to the increase.
        if (m_dependencyRefCount.count(dep) == 0) {
            sizeChange += dep->getSize();
        }
    }
    
    return (m_size + sizeChange) <= bagSize;
}

/**
 * @brief Logic for checking an N-to-1 swap.
 */
bool Bag::canSwapReadOnly(const std::vector<const Package*>& packagesIn, const Package& packageOut, int bagSize,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const noexcept
{
    int sizeChange = 0;
    
    // Create a temporary reference count map to simulate removal
    std::unordered_map<const Dependency*, int> tempRefCount = m_dependencyRefCount;
    for (const auto* p_in : packagesIn) {
        for (const auto* dep : dependencyGraph.at(p_in)) {
            if (tempRefCount.count(dep)) {
                tempRefCount[dep]--;
            }
        }
    }

    // Calculate the size reduction based on dependencies whose counts dropped to 0
    for(const auto& pair : tempRefCount) {
        const Dependency* dep = pair.first;
        int newCount = pair.second;
        int originalCount = m_dependencyRefCount.at(dep);

        if (newCount == 0 && originalCount > 0) {
            sizeChange -= dep->getSize();
        }
    }
    
    // Calculate size increase from adding packageOut, using the simulated reference counts
    for (const auto* dep : dependencyGraph.at(&packageOut)) {
        if (tempRefCount.count(dep) == 0 || tempRefCount.at(dep) == 0) {
            sizeChange += dep->getSize();
        }
    }

    return (m_size + sizeChange) <= bagSize;
}

/**
 * @brief Implementation of the new getInvalidPackages method.
 */
std::vector<const Package*> Bag::getInvalidPackages(
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph) const
{
    std::vector<const Package*> invalidPackages;
    // Check every package currently in the bag
    for (const Package* pkg : m_baggedPackages) {
        bool is_valid = true;
        // Look up its required dependencies in the provided graph
        const auto& required_deps = dependencyGraph.at(pkg);
        
        // Check if each required dependency is present in the bag's dependency set
        for (const Dependency* req_dep : required_deps) {
            if (m_baggedDependencies.count(req_dep) == 0) {
                is_valid = false;
                break;
            }
        }
        
        if (!is_valid) {
            invalidPackages.push_back(pkg);
        }
    }
    return invalidPackages;
}

// =====================================================================================
// Utility Methods
// =====================================================================================
std::string Bag::toString() const {
    ///Algorithm algoHelper(0,0);
    std::string bagString;

    //bagString += "Algorithm: " + algoHelper.toString(m_bagAlgorithm) + " | " + algoHelper.toString(m_localSearch) + "\n";
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

std::string Bag::toString(SearchEngine::MovementType movement) const
{
    switch (movement)
    {
    case SearchEngine::MovementType::ADD:
        return "ADD";
    case SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1:
        return "SWAP_REMOVE_1_ADD_1";
    case SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2:
        return "SWAP_REMOVE_1_ADD_2";
    case SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1:
        return "SWAP_REMOVE_2_ADD_1";
    default:
    return "EJECTION_CHAIN";
    }
}

std::string Bag::toString(SolutionRepair::FeasibilityStrategy feasibilityStrategy) const
{
    switch (feasibilityStrategy)
    {
    case SolutionRepair::FeasibilityStrategy::SMART:
        return "SMART";
    case SolutionRepair::FeasibilityStrategy::TEMPERATURE_BIASED:
        return "TEMPERATURE_BIASED";
    default:
    return "PROBABILISTIC_GREEDY";
    }
}

    std::vector<SearchEngine::MovementType> movements = {
        SearchEngine::MovementType::ADD,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_1,
        SearchEngine::MovementType::SWAP_REMOVE_1_ADD_2,
        SearchEngine::MovementType::SWAP_REMOVE_2_ADD_1,
        SearchEngine::MovementType::EJECTION_CHAIN
    };
