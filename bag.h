#ifndef BAG_H
#define BAG_H

#include <vector>
#include <string>
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
     * @brief Constructs a new Bag pre-filled with a list of packages.
     * @note This constructor is intended for creating a Bag from a pre-determined
     * set of packages, like a final solution. It calculates the initial size
     * and resolves dependencies from the provided packages.
     * @param packages A vector of pointers to Package objects to place in the bag.
     */
    explicit Bag(const std::vector<Package*>& packages);

    /**
     * @brief Gets the list of packages currently in the bag.
     * @return A constant reference to the vector of const Package pointers.
     */
    const std::vector<const Package*>& getPackages() const;

    /**
     * @brief Gets the list of unique dependencies required by the packages in the bag.
     * @return A constant reference to the vector of const Dependency pointers.
     */
    const std::vector<const Dependency*>& getDependencies() const;

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
     * @brief Generates a string summary of the bag's contents.
     * @return A std::string describing the bag's state.
     */
    std::string toString() const;

private:
    /**
     * @brief A helper method to add a list of dependencies to the bag, avoiding duplicates.
     * @param dependencies The vector of Dependency pointers to add.
     */
    void addDependencies(const std::unordered_map<std::string, Dependency*>& dependencies);

    Algorithm::ALGORITHM_TYPE m_bagAlgorithm;
    std::string m_timeStamp = "0000-00-00 00:00:00";
    int m_size;
    double m_algorithmTimeSeconds;
    std::vector<const Package*> m_baggedPackages;
    std::vector<const Dependency*> m_baggedDependencies;
};

#endif // BAG_H