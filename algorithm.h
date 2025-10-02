#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

class Bag;
class Package;
class Dependency;
enum class ALGORITHM_TYPE;

/**
 * @brief Provides a collection of algorithms to solve the package dependency knapsack problem.
 *
 * This class acts as a solver engine for a variation of the classic knapsack problem,
 * where items (Packages) have inter-dependencies. The goal is to select a subset
 * of packages that maximizes total benefit, fits within a capacity-limited "bag",
 * and satisfies all dependency constraints (i.e., if Package B depends on Package A,
 * B cannot be included without A). It encapsulates several heuristic strategies,
 * including greedy, random, and hybrid approaches, to find high-quality solutions. 🧠⚙️
 */
class Algorithm {
public:
    /**
     * @brief Defines the available heuristic strategies for solving the packing problem.
     *
     * This enumeration lists the different algorithms that the `run` method can execute.
     * These strategies determine how packages are prioritized and selected for inclusion in the bag,
     * ranging from simple random choice to sophisticated greedy and hybrid methods.
     */
    enum class ALGORITHM_TYPE {
        NONE,                                       ///< No algorithm specified; a placeholder.
        RANDOM,                                     ///< Purely random package selection strategy.

        // --- Greedy Strategies ---
        GREEDY_Package_Benefit,                     ///< Greedy: Prioritizes packages with the highest absolute benefit.
        GREEDY_Package_Benefit_Ratio,               ///< Greedy: Prioritizes packages with the best benefit-to-size ratio.
        GREEDY_Package_Size,                        ///< Greedy: Prioritizes packages with the smallest size.

        // --- Random-Greedy (Hybrid) Strategies ---
        RANDOM_GREEDY_Package_Benefit,              ///< Hybrid: Randomly selects from a top-candidate pool sorted by benefit.
        RANDOM_GREEDY_Package_Benefit_Ratio,        ///< Hybrid: Randomly selects from a top-candidate pool sorted by benefit-to-size ratio.
        RANDOM_GREEDY_Package_Size,                 ///< Hybrid: Randomly selects from a top-candidate pool sorted by smallest size.
        VND,                                        ///< VND
        VNS                                         ///< VNS
    };

    /**
     * @brief Constructs the Algorithm solver.
     * @param maxTime A timeout in seconds for the `run` method to prevent excessive execution time.
     */
    explicit Algorithm(double maxTime);

    /**
     * @brief Executes the selected algorithm to find solutions for the package packing problem.
     * @details This is the main entry point for the solver. Depending on the algorithm type,
     * it may return multiple solutions. For instance, a 'GREEDY' run will produce a
     * separate result for each greedy criterion (by benefit, size, and ratio).
     *
     * @warning The caller assumes ownership of the `Bag` objects returned in the vector.
     * It is the caller's responsibility to `delete` each pointer to prevent memory leaks.
     *
     * @param algorithm The specific algorithm strategy to use (e.g., GREEDY_Package_Benefit).
     * @param bagSize The maximum capacity of the bag.
     * @param packages A list of all available packages to choose from.
     * @param dependencies A list defining the dependencies between packages.
     * @param timestamp A string identifier for this execution, used for logging and tracking.
     * @return A vector of pointers to `Bag` objects, each representing a valid solution.
     */
    std::vector<Bag*> run(ALGORITHM_TYPE algorithm, int bagSize,
                          const std::vector<Package*>& packages,
                          const std::vector<Dependency*>& dependencies,
                          const std::string& timestamp);

    /**
     * @brief Converts an ALGORITHM_TYPE enum value to its string representation.
     * @param algorithm The enum value to convert.
     * @return A human-readable std::string representing the algorithm's name, useful for logging.
     */
    std::string toString(ALGORITHM_TYPE algorithm) const;

private:
    /**
     * @brief Selects and removes a random package from a list.
     * @details This function implements a pure random selection heuristic. It modifies the input
     * vector by erasing the selected element, ensuring it cannot be picked again.
     *
     * @param packageList A reference to a vector of Package pointers. This list is modified.
     * @return A pointer to the randomly selected Package, or `nullptr` if the list is empty.
     */
    Package* pickRandomPackage(std::vector<Package*>& packageList);

    /**
     * @brief Selects and removes the first package from a list.
     * @details This function implements a deterministic selection heuristic, always choosing
     * the top element. It assumes the list has been pre-sorted according to a greedy criterion.
     * It modifies the input vector by erasing the first element.
     *
     * @param packageList The list of packages to pick from. This vector will be modified.
     * @return A pointer to the top package, or `nullptr` if the list is empty.
     */
    Package* pickTopPackage(std::vector<Package*>& packageList);

    /**
     * @brief Selects and removes a random package from a top-N candidate pool.
     * @details This function implements a semi-random or GRASP-like (Greedy Randomized
     * Adaptive Search Procedure) heuristic. It assumes the list is sorted by a "best"
     * criterion. It creates a Restricted Candidate List (RCL) from the `poolSize`
     * top elements and then randomly selects one, introducing diversity into a greedy approach.
     *
     * @param packageList A reference to a vector of sorted Package pointers. This list is modified.
     * @param poolSize The number of top packages to form the random selection pool.
     * @return A pointer to the selected Package, or `nullptr` if the list is empty.
     */
    Package* pickSemiRandomPackage(std::vector<Package*>& packageList, int poolSize = 10);

    /**
     * @brief A generic worker function that fills a bag using a specified package-picking strategy.
     * @details This function implements a template method pattern. It iteratively calls the
     * `pickStrategy` function to select the next candidate package, checks if it can be added
     * (capacity and dependencies), and adds it to the bag if valid. This loop continues until
     * no more packages can be added from the list.
     *
     * @param bagSize The maximum capacity of the bag.
     * @param packages The list of available packages to select from. This list will be consumed.
     * @param pickStrategy A function pointer or lambda that encapsulates the selection logic (e.g., pickTopPackage).
     * @param type The algorithm type being run, used for specific logic branches if needed.
     * @return A pointer to the newly created and filled Bag.
     */
    Bag* fillBagWithStrategy(int bagSize, std::vector<Package*>& packages,
                             std::function<Package*(std::vector<Package*>&)> pickStrategy,
                             ALGORITHM_TYPE type);

    /**
     * @brief Implements the random package selection algorithm.
     * @details This method repeatedly picks packages at random from the available list and
     * attempts to add them to the bag until no more valid placements are possible.
     */
    Bag* randomBag(int bagSize, const std::vector<Package*>& packages,
                   const std::vector<Dependency*>& dependencies);

    /**
     * @brief Creates a new vector of packages sorted by benefit in descending order.
     * @param packages The original list of packages to sort.
     */
    Bag* vndBag(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages);

    /**
     * @brief Implements the Variable Neighborhood Search (VNS) metaheuristic to improve an initial solution.
     * @details VNS explores different neighborhood structures to escape local optima. It starts with an
     * initial solution and iteratively applies a `shake` function to perturb the solution and a
     * `localSearch` function to find a new local optimum.
     *
     * @param bagSize The maximum capacity of the bag.
     * @param initialBag A pointer to the initial `Bag` solution to be improved.
     * @param allPackages A complete list of all available packages for generating neighbors.
     * @return A pointer to a new `Bag` object representing the best solution found by VNS.
     */
    Bag* vnsBag(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages);
    
    /**
     * @brief Perturbs the current solution to escape local optima by performing a series of random swaps.
     * @details This function is a key component of the VNS algorithm. It generates a neighboring solution
     * by randomly removing `k` packages from the current bag and replacing them with `k` different
     * packages that are not already in the bag, while ensuring the new combination is valid.
     *
     * @param currentBag The current `Bag` solution to be perturbed.
     * @param k The neighborhood size, indicating how many packages to swap.
     * @param allPackages A complete list of all available packages.
     * @param bagSize The maximum capacity of the bag.
     * @return A new `Bag` object representing the perturbed solution.
     */
    Bag* shake(const Bag& currentBag, int k, const std::vector<Package*>& allPackages, int bagSize);

    /**
     * @brief Improves a solution by exploring its neighborhood.
     * @details This function is part of the VNS algorithm. It systematically explores the neighborhood
     * of the `currentBag` solution, looking for a better one. If a better solution is found,
     * the `currentBag` is updated to this new, improved solution.
     *
     * @param currentBag A reference to the `Bag` solution to be improved. This object may be modified.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A complete list of all available packages for generating neighbors.
     * @return `true` if a better solution was found and `currentBag` was improved, `false` otherwise.
     */
    bool localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages);

    /**
     * @brief Systematically explores the swap neighborhood of a solution to find an improvement.
     * @details This function tries to improve the current solution by swapping one package from inside the
     * bag with one package from outside the bag. It iterates through all possible valid swaps and
     * applies the first one that results in a higher total benefit.
     *
     * @param currentBag A reference to the `Bag` solution to be improved. This object may be modified.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A complete list of all available packages.
     * @return `true` if an improved solution was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhood(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages);

    /**
     * @brief Implements the family of greedy selection algorithms.
     * @details This method generates three distinct solutions. It creates three sorted lists of
     * packages based on different criteria (benefit, size, benefit/size ratio) and then
     * deterministically fills a bag from each list, always picking the best available package.
     * @return A vector containing three `Bag` objects, one for each greedy heuristic.
     */
    std::vector<Bag*> greedyBag(int bagSize, const std::vector<Package*>& packages,
                                const std::vector<Dependency*>& dependencies);

    /**
     * @brief Implements the family of random-greedy hybrid algorithms.
     * @details This method generates three solutions using a GRASP-like approach. For each of the
     * three greedy criteria (benefit, size, ratio), it sorts the packages, then iteratively
     * builds a solution by randomly picking from a candidate list of the best available packages.
     * This introduces randomness to help escape local optima.
     * @return A vector containing three `Bag` objects, one for each random-greedy heuristic.
     */
    std::vector<Bag*> randomGreedy(int bagSize, const std::vector<Package*>& packages,
                                  const std::vector<Dependency*>& dependencies);

    /**
     * @brief Creates a new vector of packages sorted by benefit in descending order.
     * @param packages The original list of packages to sort.
     * @return A new vector containing a sorted copy of the packages.
     */
    std::vector<Package*> sortedPackagesByBenefit(const std::vector<Package*>& packages);

    /**
     * @brief Creates a new vector of packages sorted by benefit-to-size ratio in descending order.
     * @param packages The original list of packages to sort.
     * @return A new vector containing a sorted copy of the packages.
     */
    std::vector<Package*> sortedPackagesByBenefitToSizeRatio(const std::vector<Package*>& packages);
    
    /**
     * @brief Creates a new vector of packages sorted by size in ascending order.
     * @param packages The original list of packages to sort.
     * @return A new vector containing a sorted copy of the packages.
     */
    std::vector<Package*> sortedPackagesBySize(const std::vector<Package*>& packages);

    /**
     * @brief A utility function to generate a random integer within an inclusive range.
     * @param min The minimum possible value (inclusive).
     * @param max The maximum possible value (inclusive).
     * @return A random integer between min and max.
     */
    int randomNumberInt(int min, int max);

    /**
     * @brief Pre-processes the dependency list into an efficient lookup structure.
     * @details This function can be used to build an adjacency list or map representation
     * of the dependency graph, allowing for O(1) or O(log n) lookup of a package's
     * dependencies instead of iterating through the full list every time.
     *
     * @param packages A constant reference to a vector of `Package` pointers, representing all available packages.
     *                 This list is used to initialize the dependency graph structure.
     * @param dependencies A constant reference to a vector of `Dependency` pointers, where each pointer
     *                     indicates a prerequisite relationship between two packages.
     */
    void precomputeDependencyGraph(const std::vector<Package*>& packages,
                                   const std::vector<Dependency*>& dependencies);

    /**
     * @brief Validates if a package can be added to a bag.
     * @details This function performs two primary checks:
     * 1.  **Capacity Check:** Ensures the package's size does not exceed the bag's remaining capacity.
     * 2.  **Dependency Check:** Verifies that all of the package's prerequisite dependencies are already present in the bag.
     * It uses memoization via the `cache` parameter to avoid re-evaluating the same package, improving performance.
     *
     * @param bag The current state of the bag being filled.
     * @param package The candidate package to be added.
     * @param maxCapacity The total capacity of the bag.
     * @param cache A map to store and retrieve the results of previous checks, avoiding redundant computation.
     * @return `true` if the package respects capacity and dependency constraints, `false` otherwise.
     */
    bool canPackageBeAdded(const Bag& bag, const Package& package, int maxCapacity,
                           std::unordered_map<const Package*, bool>& cache);

    const double m_maxTime;       ///< A timeout in seconds for the `run` method.
    std::string m_timestamp = "0";///< A string identifier for a specific `run` execution.
};

#endif // ALGORITHM_H