#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <vector>
#include <string>
#include <map>

// Forward declarations for pointer-based struct
class Package;
class Dependency;

/**
 * @brief Holds the data parsed from a problem instance file.
 *
 * NOTE: This struct contains pointers to objects created on the heap.
 * The code that calls loadProblem() is responsible for managing
 * and deleting the memory for all Packages and Dependencies.
 */
struct ProblemInstance {
    int maxCapacity = 0;
    std::vector<Package*> packages;
    std::vector<Dependency*> dependencies;
};

/**
 * @brief Holds the data parsed from a solution report file.
 * Used for solution validation.
 */
struct SolutionReport {
    long reportedBenefit = 0;
    long reportedWeight = 0;
    std::vector<int> packageVector;
    std::vector<int> dependencyVector;
};

/**
 * @brief Encapsulates the complete results of the validation process.
 */
struct ValidationResult {
    // Calculated values
    long calculatedBenefit = 0;
    int packageCount = 0;
    long calculatedReportWeight = 0;
    int reportedDependencyCount = 0;
    size_t trueRequiredDependencyCount = 0;
    long trueWeight = 0;

    // Validation flags
    bool isBenefitValid = false;
    bool isReportedWeightValid = false;
    bool isConsistent = false;
    bool isFeasible = false;

    /**
     * @brief Checks if the overall solution is valid.
     * @return True if the solution is both consistent and feasible, false otherwise.
     */
    [[nodiscard]] bool isOverallValid() const {
        return isConsistent && isFeasible;
    }
};

#endif // DATA_STRUCTURES_H