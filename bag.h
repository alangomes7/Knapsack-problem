#ifndef BAG_H
#define BAG_H

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "algorithm.h"

class Package;
class Dependency;

/**
 * @brief Represents a collection of software packages and their dependencies.
 *
 * A Bag is like a knapsack that holds a selection of packages chosen by an
 * algorithm. It tracks the total size of its contents, the packages themselves,
 * and the dependencies they introduce. It also records which algorithm was used
 * to fill it and how long the process took. 🛍️
 */
class Bag {
public:
    /**
     * @brief Constructs a new, empty Bag.
     * @param bagAlgorithm The type of algorithm that will be used to fill this bag.
     * @param timestamp The timestamp indicating when this bag was created.
     */
    explicit Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp);

    /**
     * @brief Constructs a new Bag by populating it from a vector of packages.
     * @note This constructor is intended for creating a Bag from a pre-determined
     * set of packages. It populates the internal set, calculates the initial size,
     * and resolves dependencies from the provided packages.
     * @param packages A vector of pointers to Package objects to place in the bag.
     */
    explicit Bag(const std::vector<Package*>& packages);

    /**
     * @brief Gets the set of packages currently in the bag.
     * @return A constant reference to the unordered_set of const Package pointers.
     */
    const std::unordered_set<const Package*>& getPackages() const;

    /**
     * @brief Gets the set of unique dependencies required by the packages in the bag.
     * @return A constant reference to the unordered_set of const Dependency pointers.
     */
    const std::unordered_set<const Dependency*>& getDependencies() const;

    /**
     * @brief Gets the total combined size of all dependencies in the bag.
     * @return The total size as an integer.
     */
    int getSize() const;

    /**
     * @brief Gets the total combined benefit of all packages in the bag.
     * @return The total benefit as an integer.
     */
    int getBenefit() const;

    /**
     * @brief Gets the type of algorithm used to create this bag's contents.
     * @return The ALGORITHM_TYPE enum value.
     */
    Algorithm::ALGORITHM_TYPE getBagAlgorithm() const;

    /**
     * @brief Gets the type of local search algorithm used on this bag.
     * @return The LOCAL_SEARCH enum value.
     */
    Algorithm::LOCAL_SEARCH getBagLocalSearch() const;

    /**
     * @brief Gets the execution time of the algorithm that filled the bag.
     * @return The time in seconds as a double.
     */
    double getAlgorithmTime() const;

    /**
     * @brief Gets the formatted timestamp for when the algorithm finished.
     * @return A string representing the timestamp (e.g., "YYYY-MM-DD HH:MM:SS").
     */
    std::string getTimestamp() const;

    /**
     * @brief Sets the execution time of the algorithm.
     * @param seconds The execution time in seconds.
     */
    void setAlgorithmTime(double seconds);

    /**
     * @brief Sets the local search algorithm type for the bag.
     * @param localSearch The local search algorithm type to set.
     */
    void setLocalSearch(Algorithm::LOCAL_SEARCH localSearch);

    /**
     * @brief Sets the algorithm type for the bag.
     * @param bagAlgorithm The algorithm type to set.
     */
    void setBagAlgorithm(Algorithm::ALGORITHM_TYPE bagAlgorithm);

    /**
     * @brief Attempts to add a package to the bag.
     *
     * This method will add the package and its unresolved dependencies. It checks
     * for duplicates to ensure the package is not already in the bag.
     * @param package The package to add.
     * @return True if the package was successfully added, false otherwise (e.g., if it was a duplicate).
     */
    bool addPackage(const Package& package);

    /**
     * @brief Removes a package and its now-unneeded dependencies from the bag.
     * @param package The package to remove.
     */
    void removePackage(const Package& package);

    /**
     * @brief Checks if a given package can be added without exceeding the maximum capacity.
     *
     * This is a predictive check and does not modify the bag. It calculates the
     * potential new size if the package and its new dependencies were added.
     * @param package The package to check.
     * @param maxCapacity The maximum capacity constraint.
     * @return True if the package would fit, false otherwise.
     */
    bool canAddPackage(const Package& package, int maxCapacity) const;

    /**
     * @brief Check if swapping a package inside the bag with a package outside is feasible.
     *
     * This method evaluates the feasibility of a swap without modifying the bag by
     * calculating the net change in size based on shared and unique dependencies.
     *
     * @param packageIn  The package currently in the bag that will be removed.
     * @param packageOut The package currently outside the bag that will be added.
     * @param bagSize    Maximum capacity of the bag.
     * @return true if the swap is feasible, false otherwise.
     */
    bool canSwap(const Package& packageIn, const Package& packageOut, int bagSize) const;

    /**
     * @brief Generates a string summary of the bag's contents.
     * @return A std::string describing the bag's state.
     */
    std::string toString() const;

private:
    Algorithm::ALGORITHM_TYPE m_bagAlgorithm;
    Algorithm::LOCAL_SEARCH m_localSearch;
    std::string m_timeStamp = "0000-00-00 00:00:00";
    int m_size;
    int m_benefit; // OPTIMIZATION: Cached total benefit
    double m_algorithmTimeSeconds;

    std::unordered_set<const Package*> m_baggedPackages;
    std::unordered_set<const Dependency*> m_baggedDependencies;
    // OPTIMIZATION: Map to track how many packages require each dependency.
    std::unordered_map<const Dependency*, int> m_dependencyRefCount;
};

#endif // BAG_H