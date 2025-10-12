#ifndef GRASP_H
#define GRASP_H

#include <vector>
#include <random>
#include <unordered_map>

#include "algorithm.h"
#include "localsearch.h"
#include "metaheuristichelper.h"

// Forward
class Bag;
class Package;
class Dependency;

class GRASP {
public:
    // Basic constructors (maxTime in seconds). Optionally pass seed for reproducibility.
    explicit GRASP(double maxTime);
    GRASP(double maxTime, unsigned int seed);

    // Advanced constructor with ILS + reactive-alpha parameters
    GRASP(double maxTime, unsigned int seed,
          int ilsRounds, int perturbStrength,
          const std::vector<double>& reactiveAlphas);

    // Main entry
    Bag* run(int bagSize,
             const std::vector<Bag*>& initialBags,
             const std::vector<Package*>& allPackages,
             Algorithm::LOCAL_SEARCH localSearchMethod,
             const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph);

private:
    // Core phases
    Bag* construction(int bagSize,
                      const std::vector<Package*>& allPackages,
                      const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                      double alpha);

    // ILS helpers
    Bag* perturbSolution(const Bag& source, int bagSize,
                         const std::vector<Package*>& allPackages,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                         int removeCount);

    // Reactive alpha helpers
    double pickAlpha();
    void updateAlphaRewards(double alpha, double improvement);

    // Parameters & state
    const double m_maxTime;
    std::mt19937 m_generator; // Kept for std::discrete_distribution

    // ILS params
    int m_ilsRounds = 10;
    int m_perturbStrength = 2; // how many packages to remove during perturbation

    // Reactive alpha structures
    std::vector<double> m_alphas;        // candidate alphas
    std::vector<double> m_alphaProbs;    // selection probabilities
    std::vector<double> m_alphaScores;   // cumulative score / reward for each alpha
    std::vector<int>    m_alphaCounts;   // how many times each alpha used
    
    LocalSearch m_localSearch;
    MetaheuristicHelper m_helper;
};

#endif // GRASP_H