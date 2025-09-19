#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

class Bag;
class Package;
class Dependency;

/**
 * @brief Provides a collection of algorithms to solve a variation of the knapsack problem.
 *
 * This class encapsulates different strategies (random, greedy, etc.) for selecting
 * a set of packages to place into a "bag" with a limited size, while respecting
 * their inter-dependencies. The main goal is to maximize the total benefit of the
 * selected packages within the bag's capacity. 🧠⚙️
 */
class Algorithm {
public:
    /**
     * @brief Defines the available strategies for the algorithm.
     */
    enum class ALGORITHM_TYPE {
        NONE,                   ///< No algorithm specified.
        RANDOM,                 ///< Selects packages randomly.
        GREEDY,                 ///< Selects packages based on the best benefit-to-size ratio.
        RANDOM_GREEDY           ///< A hybrid approach combining greedy selection with randomness.
    };

    /**
     * @brief Constructs the Algorithm runner.
     * @param maxTime The maximum time of running.
     */
    explicit Algorithm(double maxTime);

    /**
     * @brief Executes the selected algorithm to fill a bag with packages.
     *
     * @warning This function returns a raw pointer to a dynamically allocated 'Bag' object.
     * The caller is responsible for deleting this object to prevent memory leaks. ⚠️
     *
     * @param algorithm The specific algorithm strategy to use.
     * @param bagSize The maximum capacity of the bag.
     * @param packages A list of all available packages to choose from.
     * @param dependencies A list of all dependencies in the system.
     * @return A pointer to the newly created Bag containing the selected packages.
     */
    Bag* run(ALGORITHM_TYPE algorithm, int bagSize,
             const std::vector<Package*>& packages,
             const std::vector<Dependency*>& dependencies);

    /**
     * @brief Converts an ALGORITH_TYPE enum value to its string representation.
     * @param algorithm The enum value to convert.
     * @return A std::string representing the algorithm's name.
     */
    std::string toString(ALGORITHM_TYPE algorithm) const;

private:
    /**
     * @brief A generic function to fill a bag using a provided package-picking strategy.
     * @param bagSize The maximum capacity of the bag.
     * @param packages The list of available packages.
     * @param pickPackage A function object that defines the strategy for selecting the next package.
     * @param type The algorithm type being run, used for specific logic branches.
     * @return A pointer to the filled Bag.
     */
    Bag* fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                             const std::function<Package*()>& pickPackage, ALGORITHM_TYPE type);

    /**
     * @brief Implements the random selection strategy.
     */
    Bag* randomBag(int bagSize, const std::vector<Package*>& packages,
                   const std::vector<Dependency*>& dependencies);

    /**
     * @brief Implements the greedy selection strategy based on benefit.
     */
    Bag* greedyBagByBenefitToSizeRatio(int bagSize, const std::vector<Package*>& packages,
                                  const std::vector<Dependency*>& dependencies);

    /**
     * @brief Implements the random-greedy hybrid strategy.
     */
    Bag* randomGreedy(int bagSize, const std::vector<Package*>& packages,
                      const std::vector<Dependency*>& dependencies);

    /**
     * @brief Sorts packages based on their benefit-to-size ratio for the greedy algorithm.
     * @param packages The list of packages to sort.
     * @return A new vector containing the sorted packages.
     */
    std::vector<Package*> sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages);

    /**
     * @brief A helper function to generate a random integer within a given range.
     * @param min The minimum value (inclusive).
     * @param max The maximum value (inclusive).
     * @return A random integer.
     */
    int randomNumberInt(int min, int max);

    /**
     * @brief Pre-processes dependencies to build an efficient lookup structure (if necessary).
     */
    void precomputeDependencyGraph(const std::vector<Package*>& packages,
                                   const std::vector<Dependency*>& dependencies);

    /**
     * @brief Checks if a package can be added to the bag without violating capacity or dependency constraints.
     * @param bag The current state of the bag.
     * @param package The candidate package to add.
     * @param maxCapacity The bag's maximum capacity.
     * @param cache A cache to store results of previous checks to improve performance.
     * @return True if the package can be added, false otherwise.
     */
    bool canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                           std::unordered_map<const Package*, bool>& cache);

    const double m_maxTime; ///< Maximum time of running.
};

#endif // ALGORITHM_H