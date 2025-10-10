#include <string>
#include <unordered_map>
#include "algorithm.h"

// Forward declarations for classes used in function signatures
class Package;
class Dependency;
class Bag;

/**
 * @brief Loads a knapsack problem instance from a file.
 *
 * @param filePath The path to the input file.
 * @param bagSize A reference to store the capacity of the knapsack.
 * @param packages A reference to a map to store the available packages.
 * @param dependencies A reference to a map to store the available dependencies.
 */
void loadFile(const std::string& filePath, int& bagSize, 
    std::unordered_map<std::string, Package*>& packages, 
    std::unordered_map<std::string, Dependency*>& dependencies);

/**
 * @brief Prints the results of the knapsack algorithms to the console.
 *
 * @param bags A constant reference to a vector of Bag pointers, each representing a solution.
 */
void printResults(const std::vector<Bag*>& bags);

/**
 * @brief Saves the results of the knapsack algorithms to a CSV file.
 *
 * @param bags A constant reference to a vector of Bag pointers.
 * @param inputFileName The name of the original input file, used for logging.
 */
void saveData(const std::vector<Bag*>& bags, const std::string& inputFileName);

/**
 * @brief Generates a string label for a given algorithm and local search combination.
 *
 * @param algorithm An instance of the Algorithm class to access its toString methods.
 * @param algo The algorithm type.
 * @param ls The local search type (optional).
 * @return A formatted string representing the algorithm and local search strategy.
 */
std::string getAlgorithmLabel(const Algorithm& algorithm, Algorithm::ALGORITHM_TYPE algo, 
    Algorithm::LOCAL_SEARCH ls = Algorithm::LOCAL_SEARCH::NONE);

/**
 * @brief Saves a detailed report for the best solution found.
 *
 * @param bestBag A pointer to the Bag with the highest benefit.
 * @param allPackages A map of all available packages to generate the binary vector.
 * @param allDependencies A map of all available dependencies.
 * @param seed The random seed used for the experiment.
 * @param filePath The path to the original input file, used for naming the report.
 * @param timestamp The timestamp string to append to the report filename.
 */
void saveReport(const Bag* bestBag, const std::unordered_map<std::string, 
    Package*>& allPackages, const std::unordered_map<std::string, 
    Dependency*>& allDependencies, unsigned int seed, const std::string& filePath, 
    const std::string& timestamp);

/**
 * @brief Converts a timestamp string to a filename-friendly format.
 * * Replaces spaces and colons with underscores.
 * Example: "2025-10-10 08:36:43" becomes "2025-10-10_08_36_43".
 *
 * @param timestamp The input timestamp string.
 * @return A new string with the converted format.
 */
std::string formatTimestampForFilename(std::string timestamp);