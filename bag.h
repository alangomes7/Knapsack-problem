#ifndef BAG_H
#define BAG_H

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "algorithm.h"

class Package;
class Dependency;
class Algorithm;

class Bag {
public:
    /**
     * @brief Constructs an empty Bag.
     * @param bagAlgorithm The algorithm type used to populate the bag.
     * @param timestamp The creation timestamp for the bag.
     */
    explicit Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp);

    /**
     * @brief Constructs a Bag from a vector of packages using a precomputed dependency graph.
     *
     * This constructor is highly efficient because it leverages a pre-built map of
     * dependencies. Instead of querying each package for its dependencies individually
     * (which is slower), it performs a fast lookup in the provided graph and calls
     * the optimized `addPackage` method.
     *
     * @param packages The vector of Package pointers to add to the bag.
     * @param dependencyGraph A constant reference to the precomputed dependency graph.
     */
    explicit Bag(const std::vector<Package*>& packages,
                 const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    /**
     * @brief Gets the set of packages currently in the bag.
     * @return A constant reference to the unordered_set of Package pointers.
     */
    const std::unordered_set<const Package*>& getPackages() const;

    /**
     * @brief Gets the set of unique dependencies for all packages in the bag.
     * @return A constant reference to the unordered_set of Dependency pointers.
     */
    const std::unordered_set<const Dependency*>& getDependencies() const;

    /**
     * @brief Gets the total size of the bag.
     * The size is the sum of the sizes of all unique dependencies.
     * @return The total size of the bag as an integer.
     */
    int getSize() const;

    /**
     * @brief Gets the total benefit of the bag.
     * The benefit is the sum of the benefits of all packages in the bag.
     * @note This is an O(1) operation as the benefit is cached.
     * @return The total benefit of the bag as an integer.
     */
    int getBenefit() const;

    /**
     * @brief Gets the primary algorithm used to create the bag's contents.
     * @return The Algorithm::ALGORITHM_TYPE enum value.
     */
    Algorithm::ALGORITHM_TYPE getBagAlgorithm() const;

    /**
     * @brief Gets the local search algorithm applied to the bag.
     * @return The Algorithm::LOCAL_SEARCH enum value.
     */
    Algorithm::LOCAL_SEARCH getBagLocalSearch() const;

    /**
     * @brief Gets the execution time of the algorithm used to fill the bag.
     * @return The algorithm's execution time in seconds.
     */
    double getAlgorithmTime() const;

    /**
     * @brief Gets the timestamp of when the bag was created.
     * @return A string representing the creation timestamp.
     */
    std::string getTimestamp() const;

    /**
     * @brief Gets the parameters used for the metaheuristic algorithm.
     * @return A constant reference to a string containing the parameters.
     */
    const std::string& getMetaheuristicParameters() const;

    /**
     * @brief Sets the bag's timestamp.
     * @param timestamp The creation timestamp for the bag.
     */
    void setTimestamp(const std::string& timestamp);

    /**
     * @brief Sets the execution time of the algorithm.
     * @param seconds The execution time in seconds.
     */
    void setAlgorithmTime(double seconds);

    /**
     * @brief Sets the local search strategy used.
     * @param localSearch The local search algorithm to set.
     */
    void setLocalSearch(Algorithm::LOCAL_SEARCH localSearch);

    /**
     * @brief Sets the primary algorithm used.
     * @param bagAlgorithm The main algorithm to set.
     */
    void setBagAlgorithm(Algorithm::ALGORITHM_TYPE bagAlgorithm);

    /**
     * @brief Sets the metaheuristic parameters string.
     * @param params A string detailing the parameters used.
     */
    void setMetaheuristicParameters(const std::string& params);

    /**
     * @brief Adds a package to the bag using a pre-fetched list of its dependencies.
     * This is an optimized method that avoids redundant dependency lookups.
     * @param package The package to add.
     * @param dependencies A vector of the package's dependencies.
     * @return True if the package was successfully added, false if it was already in the bag.
     */
    bool addPackage(const Package& package, const std::vector<const Dependency*>& dependencies);

    /**
     * @brief Removes a package from the bag using a pre-fetched list of its dependencies.
     *
     * This version is more efficient as it iterates over a vector of dependencies,
     * which benefits from better memory locality compared to the unordered_map in the Package class.
     *
     * @param package The package to remove.
     * @param dependencies A vector containing pointers to the dependencies of the package.
     */
    void removePackage(const Package& package, const std::vector<const Dependency*>& dependencies);

    /**
     * @brief Checks if a package can be added to the bag without exceeding a maximum capacity.
     * @param package The package to check.
     * @param maxCapacity The maximum capacity constraint of the bag.
     * @param dependencies A pre-fetched vector of the package's dependencies.
     * @return True if the package can be added, false otherwise.
     */
    bool canAddPackage(const Package& package, int maxCapacity, const std::vector<const Dependency*>& dependencies) const;

    /**
     * @brief Determines if swapping two packages is feasible without exceeding the bag's size limit.
     * This method calculates the net change in size from removing one package and adding another.
     * @param packageIn The package to be removed from the bag.
     * @param packageOut The package to be added to the bag.
     * @param bagSize The maximum capacity of the bag.
     * @return True if the swap is valid within the size constraints, false otherwise.
     */
    bool canSwap(const Package& packageIn, const Package& packageOut, int bagSize) const;

    /**
     * @brief Generates a formatted string summarizing the bag's contents and statistics.
     * @return A string containing details like algorithm, size, benefit, time, and package list.
     */
    std::string toString() const;

private:
    Algorithm::ALGORITHM_TYPE m_bagAlgorithm;
    Algorithm::LOCAL_SEARCH m_localSearch;
    std::string m_timeStamp = "0000-00-00 00:00:00";
    int m_size;
    int m_benefit;
    double m_algorithmTimeSeconds;
    std::string m_metaheuristicParams;

    std::unordered_set<const Package*> m_baggedPackages;
    std::unordered_set<const Dependency*> m_baggedDependencies;
    std::unordered_map<const Dependency*, int> m_dependencyRefCount;
};

#endif // BAG_H