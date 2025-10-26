#ifndef DATA_MODEL_H
#define DATA_MODEL_H

// Standard library headers
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <iomanip>
#include <unordered_map>

// Forward declarations for pointer-based struct
class Package;
class Dependency;

/**
 * @brief Holds the data parsed from a problem instance file.
 *
 * NOTE: This struct now manages the lifetime of the objects it
 * points to. It will create deep copies when copied and
 * delete its objects when destroyed.
 */
struct ProblemInstance {
    // --- Public Data ---
    int maxCapacity = 0;
    std::vector<Package*> packages;
    std::vector<Dependency*> dependencies;
    std::unordered_map<std::string, Dependency*> dependencyMap;

    // --- Constructors / Destructor / Assignment ---
    ProblemInstance();
    ~ProblemInstance();
    ProblemInstance(const ProblemInstance& other);
    ProblemInstance& operator=(const ProblemInstance& other);

    // --- Public Methods ---

    /**
     * @brief Populates the dependencyMap for quick lookups by name.
     */
    void buildDependencyMap();

    /**
     * @brief Generates a string summary of the problem instance.
     * @return A string with capacity, package count, and dependency count.
     */
    [[nodiscard]] std::string toString() const;

    // --- Const Getters ---
    [[nodiscard]] const std::vector<Package*>& getPackages() const {
        return packages;
    }

    [[nodiscard]] const std::vector<Dependency*>& getDependencies() const {
        return dependencies;
    }

    // --- Non-const Getters ---
    std::vector<Package*>& getPackages() {
        return packages;
    }

    std::vector<Dependency*>& getDependencies() {
        return dependencies;
    }

private:
    /**
     * @brief Deletes all heap-allocated Package and Dependency objects.
     */
    void clear();
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

    /**
     * @brief Generates a string representation of the solution report.
     * @return A formatted string summarizing the struct's fields.
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "SolutionReport {\n"
            << "  reportedBenefit: " << reportedBenefit << "\n"
            << "  reportedWeight: " << reportedWeight << "\n"
            << "  packageVector (count): " << packageVector.size() << "\n"
            << "  dependencyVector (count): " << dependencyVector.size() << "\n"
            << "}";
        // Note: Not printing full vector contents to avoid large output.
        return oss.str();
    }
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

    /**
     * @brief Generates a string representation of the validation results.
     * @return A formatted string summarizing the struct's fields.
     */
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << std::boolalpha;
        oss << "ValidationResult {\n"
            << "  Overall Valid: " << isOverallValid() << "\n"
            << "  --------------------\n"
            << "  Flags:\n"
            << "    isConsistent: " << isConsistent << "\n"
            << "    isFeasible: " << isFeasible << "\n"
            << "    isBenefitValid: " << isBenefitValid << "\n"
            << "    isReportedWeightValid: " << isReportedWeightValid << "\n"
            << "  --------------------\n"
            << "  Calculated Values:\n"
            << "    calculatedBenefit: " << calculatedBenefit << "\n"
            << "    trueWeight: " << trueWeight << "\n"
            << "    packageCount: " << packageCount << "\n"
            << "    trueRequiredDependencyCount: " << trueRequiredDependencyCount << "\n"
            << "  --------------------\n"
            << "  Reported/Internal Values:\n"
            << "    calculatedReportWeight: " << calculatedReportWeight << "\n"
            << "    reportedDependencyCount: " << reportedDependencyCount << "\n"
            << "}";
        return oss.str();
    }
};

#endif // DATA_MODEL_H