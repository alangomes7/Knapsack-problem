#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>

#include "data_model.h"

// Forward declaration for Bag (still needed here)
class Bag;
// Package and Dependency are now forward-declared in DataStructures.h

/**
 * @brief Contains utilities for loading problem files, loading solution reports,
 * and saving new experiment results and reports to disk.
 */
namespace FILE_PROCESSOR {

/**
 * @brief Loads a problem instance from a specified file.
 *
 * This function creates Package and Dependency objects on the heap.
 * The caller is responsible for their memory management (deletion).
 *
 * @param filename The path to the problem file.
 * @return A ProblemInstance struct containing the loaded data.
 * @throws std::runtime_error if the file cannot be opened or is malformed.
 */
ProblemInstance loadProblem(const std::string& filename);

/**
 * @brief Appends the summary of results from a vector of Bags
 * to a CSV file.
 *
 * @param bags The vector of Bag pointers containing solution data.
 * @param outputDir The directory to save the file in.
 * @param inputFilename The name of the original problem file (for logging).
 * @param fileId The id of the file to save in. It is used in case of multiple run to keep the results separated.
 */
void saveData(const std::vector<std::unique_ptr<Bag>>& bags,
              const std::string& outputDir,
              const std::string& inputFilename,
              const std::string& fileId);

/**
 * @brief Saves a detailed .txt report for the bag solution,
 * which can be used for reproduction and validation.
 *
 * @param bag Solution bag from the experiment.
 * @param allPackages Vector of all possible packages in the problem.
 * @param allDependencies Vector of all possible dependencies.
 * @param seed The random seed used for the experiment.
 * @param timestamp The timestamp when the experiment was run.
 * @param outputDir The directory to save the report file in.
 * @param inputFilename The name of the original problem file.
 * @returns report file.
 */
std::string saveReport(const std::unique_ptr<Bag>& bag,
                const std::vector<Package*>& allPackages,
                const std::vector<Dependency*>& allDependencies,
                unsigned int seed,
                const std::string& timestamp,
                const std::string& outputDir,
                const std::string& inputFilename);
/**
 * @brief Loads a previously generated solution report file for validation.
 *
 * @param filename The path to the report file.
 * @return A SolutionReport struct containing the data from the report.
 * @throws std::runtime_error if the file cannot be opened.
 */
SolutionReport loadReport(const std::string& filename);


/**
 * @brief Validates a solution report against a problem instance file.
 *
 * Checks for:
 * 1. Feasibility (Dependencies): All required dependencies for selected
 * packages are included, and no unused dependencies are included.
 * 2. Feasibility (Capacity): Total weight of dependencies does not exceed
 * max capacity.
 * 3. Correctness (Benefit/Weight): Reported values match calculated values.
 *
 * @param problemFilename The path to the original problem file (.txt).
 * @param reportFilename The path to the solution report file (.txt).
 * @return true if the solution is valid, false otherwise.
 */
std::string validateSolution(const std::string& problemFilename,
                                   const std::string& reportFilename);

/**
 * @brief Converts a path string using backslashes (\) to one
 * using forward slashes (/).
 */
std::string backslashesPathToSlashesPath(const std::string& s);

/**
 * @brief Formats a standard timestamp string (e.g., "YYYY-MM-DD HH:MM:SS")
 * into a string safe for use in a filename
 * (e.g., "YYYY-MM-DD_HH-MM-SS").
 */
std::string formatTimestampForFileName(const std::string& ts);

/**
 * @brief Creates a unique output directory. If 'base' exists,
 * it tries 'base-1', 'base-2', etc., until a new
 * directory can be created.
 *
 * @param base The desired base name for the directory.
 * @return The path of the successfully created directory.
 */
std::string createUniqueOutputDir(const std::string& base);

} // namespace FILE_PROCESSOR

#endif // FILE_PROCESSOR_H