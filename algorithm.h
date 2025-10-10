#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>

class Bag;
class Package;
class Dependency;
enum class ALGORITHM_TYPE;
enum class LOCAL_SEARCH;

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
        VND,                                        ///< Variable Neighborhood Descent metaheuristic.
        VNS                                         ///< Variable Neighborhood Search metaheuristic.
    };

    /**
     * @brief Defines the strategies for the local search phase within metaheuristics.
     * @details This enumeration specifies how the neighborhood of a solution is explored
     * to find an improvement.
     */
    enum class LOCAL_SEARCH {
        FIRST_IMPROVEMENT,                          ///< Applies the first improvement found while exploring the neighborhood.
        BEST_IMPROVEMENT,                           ///< Explores the entire neighborhood and applies the best improvement found.
        RANDOM_IMPROVEMENT,                         ///< Finds all possible improvements and applies one at random.
        NONE                                        ///< No local search is performed.
    };

    /**
     * @brief Constructs the Algorithm solver.
     * @param maxTime A timeout in seconds for the `run` method to prevent excessive execution time.
     */
    explicit Algorithm(double maxTime);
    explicit Algorithm(double maxTime, unsigned int seed);

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
    
    /**
     * @brief Converts a LOCAL_SEARCH enum value to its string representation.
     * @param localSearch The enum value to convert.
     * @return A human-readable std::string representing the local search strategy's name.
     */
    std::string toString(LOCAL_SEARCH localSearch) const;

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
     * @brief Efficiently fills a bag using a given package selection strategy.
     *
     * @details
     * This function implements the constructive phase of heuristics. It repeatedly:
     *   1. Uses the provided `pickStrategy` to select a candidate package from the pool.
     *   2. Checks whether the package can be added without exceeding capacity or violating dependency constraints.
     *   3. Adds the package if feasible.
     *
     * Performance optimizations:
     *   - Uses a function pointer instead of std::function to avoid virtual dispatch.
     *   - Avoids expensive vector erase operations when possible (swap-and-pop is recommended).
     *   - Caches feasibility checks (`canAddPackage`) in an unordered_map to skip repeated evaluations.
     *   - Throttles time-limit checks (every N iterations instead of each loop) to reduce chrono overhead.
     *   - Pre-reserves cache memory proportional to the number of packages.
     *
     * The loop terminates when either:
     *   - The bag reaches the maximum allowed size,
     *   - No valid package can be added,
     *   - The given time budget (`m_maxTime`) is exceeded.
     *
     * @param bagSize The maximum capacity of the bag (size constraint).
     * @param packages The pool of candidate packages (consumed during execution).
     *                 The strategy may mutate this container (e.g., via swap-and-pop).
     * @param pickStrategy A pointer to a selection function (e.g., pickTopPackage, pickRandomPackage).
     *                     It must return a pointer to the chosen package or nullptr if none remain.
     * @param type The algorithm type being executed (metadata stored in the Bag).
     * @return A pointer to a newly constructed and filled Bag. Caller owns the memory.
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
     * @brief Implements the Variable Neighborhood Descent (VND) metaheuristic to improve a solution.
     * @details VND is a local search method that systematically explores a set of predefined
     * neighborhood structures. It starts with an initial solution and searches for improvements
     * in the first neighborhood. If an improvement is found, the search restarts from that
     * new solution in the first neighborhood. If not, it moves to the next neighborhood
     * structure. This continues until all neighborhoods have been explored without finding
     * an improvement.
     *
     * @param bagSize The maximum capacity of the bag.
     * @param initialBag A pointer to the initial `Bag` solution to be improved.
     * @param allPackages A complete list of all available packages for generating neighbors.
     * @return A pointer to a new `Bag` object representing the best solution found by VND.
     */
    Bag* vndBag(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages);

    /**
     * @brief Implements the Variable Neighborhood Search (VNS) metaheuristic to improve a solution.
     * @details VNS is designed to escape local optima by systematically changing neighborhood
     * structures during the search. It works by repeatedly executing three steps:
     * 1. Shaking: Perturb the current solution to jump to a random neighbor in the k-th neighborhood.
     * 2. Local Search: Apply a local search method to find a new local optimum from the perturbed solution.
     * 3. Move: If the new optimum is better, move to it. Otherwise, stay at the current solution.
     * The neighborhood distance `k` is increased if no improvement is found.
     *
     * @param bagSize The maximum capacity of the bag.
     * @param initialBag A pointer to the initial `Bag` solution to improve.
     * @param allPackages A complete list of all available packages.
     * @param localSearchMethod The local search strategy (e.g., BEST_IMPROVEMENT) to apply after shaking.
     * @return A pointer to a new `Bag` object representing the best solution found by VNS.
     */
    Bag* vnsBag(int bagSize, Bag* initialBag, const std::vector<Package*>& allPackages, LOCAL_SEARCH localSearchMethod);

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
     * @brief Applies a local search heuristic to improve a given solution.
     * @details This function acts as a dispatcher, calling the appropriate neighborhood
     * exploration method (e.g., First Improvement, Best Improvement) based on the
     * `localSearchMethod` parameter. It modifies the `currentBag` in place if
     * an improvement is found.
     *
     * @param currentBag The bag solution to be improved, passed by reference.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A complete list of all packages available for swaps.
     * @param localSearchMethod The specific local search strategy to apply.
     * @return `true` if the solution was improved, `false` otherwise.
     */
    bool localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages, LOCAL_SEARCH localSearchMethod);

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
    bool exploreSwapNeighborhoodFirstImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages);

    /**
     * @brief Systematically explores the swap neighborhood and applies the best found improvement.
     * @details This function evaluates all possible valid swaps of one package from inside the bag
     * with one package from outside. It then applies the swap that results in the greatest
     * increase in total benefit.
     *
     * @param currentBag A reference to the `Bag` solution to be improved.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A complete list of all available packages.
     * @return `true` if an improved solution was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhoodBestImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages);

    /**
     * @brief Explores the swap neighborhood and applies a randomly selected improvement.
     * @details This function identifies all possible valid swaps that would improve the solution's
     * total benefit. From this list of improvements, it randomly selects one to apply,
     * introducing a non-deterministic element to the local search.
     *
     * @param currentBag A reference to the `Bag` solution to be improved.
     * @param bagSize The maximum capacity of the bag.
     * @param allPackages A complete list of all available packages.
     * @return `true` if an improved solution was found and applied, `false` otherwise.
     */
    bool exploreSwapNeighborhoodRandomImprovement(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages);

    /**
     * @brief Evaluate a potential swap between an inside-package and an outside-package.
     *
     * @param currentBag   The current solution bag.
     * @param packageIn    Package currently in the bag (to remove).
     * @param packageOut   Package outside the bag (to add).
     * @param bagSize      Maximum allowed bag size.
     * @param currentBenefit Cached current total benefit of the bag.
     * @param[out] benefitIncrease Stores the computed benefit increase if feasible.
     *
     * @return true if the swap is feasible AND strictly improves benefit, false otherwise.
     */
    bool evaluateSwap(const Bag& currentBag, const Package* packageIn, Package* packageOut, int bagSize,
                        int currentBenefit, int& benefitIncrease) const;

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
     * @brief Iteratively removes packages from a bag until its size is within a specified maximum.
     * @details This function implements a greedy repair strategy. In a loop, it identifies the
     * package with the worst benefit-to-size ratio and removes it. This process is
     * repeated until the bag's total size is less than or equal to the maxCapacity.
     * This is useful for ensuring solutions generated by methods like 'shake' remain valid.
     * @param bag The Bag object to modify, passed by reference.
     * @param maxCapacity The target maximum size for the bag.
     * @return true if the bag was successfully brought within the size limit, false otherwise.
     */
    bool removePackagesToFit(Bag& bag, int maxCapacity);

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
     * This list is used to initialize the dependency graph structure.
     * @param dependencies A constant reference to a vector of `Dependency` pointers, where each pointer
     * indicates a prerequisite relationship between two packages.
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
    std::mt19937 m_generator;
};

#endif // ALGORITHM_H