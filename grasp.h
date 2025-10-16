#ifndef GRASP_H
#define GRASP_H

#include <vector>
#include <random>
#include <unordered_map>

#include "algorithm.h"
#include "SearchEngine.h"
#include "metaheuristicHelper.h"

// Forward declarations
class Bag;
class Package;
class Dependency;

/**
 * @brief Implements the Greedy Randomized Adaptive Search Procedure (GRASP) metaheuristic.
 *
 * This class provides an enhanced GRASP implementation for the knapsack problem, incorporating
 * features like Iterated Local Search (ILS), reactive alpha parameter tuning, path relinking,
 * and elite set management to enhance solution quality and exploration.
 */
class GRASP {
public:
    /**
     * @brief Basic constructor for GRASP.
     * @param maxTime The maximum execution time in seconds.
     */
    explicit GRASP(double maxTime);
    
    /**
     * @brief Constructor for GRASP with a specific seed for reproducibility.
     * @param maxTime The maximum execution time in seconds.
     * @param seed The seed for the random number generator.
     */
    GRASP(double maxTime, unsigned int seed);

    /**
     * @brief Advanced constructor with ILS and reactive-alpha parameters.
     * @param maxTime The maximum execution time in seconds.
     * @param seed The seed for the random number generator.
     * @param ilsRounds The number of Iterated Local Search rounds to perform.
     * @param perturbStrength The strength of the perturbation in the ILS phase.
     * @param reactiveAlphas A vector of alpha values for the reactive GRASP.
     */
    GRASP(double maxTime, unsigned int seed,
          int ilsRounds, int perturbStrength,
          const std::vector<double>& reactiveAlphas);

    /**
     * @brief Runs the GRASP algorithm with enhanced exploration strategies.
     * @param bagSize The maximum capacity of the knapsack.
     * @param initialBags A vector of initial solutions to start from.
     * @param allPackages A vector of all available packages.
     * @param localSearchMethod The local search method to use.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the best found solution (Bag).
     */
    Bag* run(int bagSize,
             const std::vector<Bag*>& initialBags,
             const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    /**
     * @brief The construction phase of the GRASP algorithm with multiple greedy criteria.
     * @param bagSize The maximum capacity of the knapsack.
     * @param allPackages A vector of all available packages.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @param alpha The alpha parameter for the Restricted Candidate List (RCL).
     * @return A pointer to the constructed solution (Bag).
     */
    Bag* construction(int bagSize,
                      const std::vector<Package*>& allPackages,
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                      double alpha);

    /**
     * @brief Perturbs a solution for the Iterated Local Search (ILS) with improved strategy.
     * @param source The source solution to perturb.
     * @param bagSize The maximum capacity of the knapsack.
     * @param allPackages A vector of all available packages.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @param removeCount The number of packages to remove during perturbation.
     * @return A pointer to the perturbed solution (Bag).
     */
    Bag* perturbSolution(const Bag& source, int bagSize,
                         const std::vector<Package*>& allPackages,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                         int removeCount);

    /**
     * @brief Performs path relinking between two solutions.
     * @param source The starting solution.
     * @param target The target solution.
     * @param bagSize The maximum capacity of the knapsack.
     * @param allPackages A vector of all available packages.
     * @param dependencyGraph A precomputed graph of package dependencies.
     * @return A pointer to the best solution found along the path.
     */
    Bag* pathRelink(const Bag& source, const Bag& target, int bagSize,
                    const std::vector<Package*>& allPackages,
                    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

    /**
     * @brief Updates the elite set with a new candidate solution.
     * @param eliteSet The current elite set of solutions.
     * @param candidate The candidate solution to potentially add to the elite set.
     */
    void updateEliteSet(std::vector<Bag*>& eliteSet, Bag* candidate);

    /**
     * @brief Picks an alpha value based on the current probabilities.
     * @return The selected alpha value.
     */
    double pickAlpha();

    /**
     * @brief Updates the rewards for the alpha values based on the improvement.
     * @param alpha The alpha value used.
     * @param improvement The improvement achieved with the alpha value.
     */
    void updateAlphaRewards(double alpha, double improvement);

    /**
     * @brief Smooths alpha probabilities to prevent premature convergence.
     * @param uniformWeight The weight of the uniform distribution in the smoothing.
     */
    void smoothAlphaProbs(double uniformWeight);

    // Parameters & state
    const double m_maxTime;
    std::mt19937 m_generator;

    // ILS params
    int m_ilsRounds;
    int m_perturbStrength;

    // Elite set and path relinking
    size_t m_eliteSetSize;
    bool m_usePathRelinking;

    // Reactive alpha structures
    std::vector<double> m_alphas;
    std::vector<double> m_alphaProbs;
    std::vector<double> m_alphaScores;
    std::vector<int>    m_alphaCounts;
    
    SearchEngine m_iteratedLocalSearch;
    MetaheuristicHelper m_helper;
};

#endif // GRASP_H