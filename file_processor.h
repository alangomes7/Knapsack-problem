#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

// Project-specific headers
#include "data_model.h"
#include "bag.h"
#include "dependency.h"
#include "package.h"

// Standard library headers
#include <string>
#include <vector>
#include <memory>

// Forward declaration for Bag
class Bag;

/**
 * @brief Contains utilities for loading problem files, loading solution reports,
 * and saving new experiment results and reports to disk.
 */
namespace FILE_PROCESSOR {

/**
 * @brief Loads a problem instance from a specified file.
 *
 * This function loads a problem instance and returns a ProblemInstance
 * struct. The ProblemInstance struct manages the memory (RAII)
 * of the loaded Package and Dependency objects.
 *
 * @param filename The path to the problem file.
 * @return A ProblemInstance struct containing the loaded data.
 * @throws std::runtime_error if the file cannot be opened or is malformed.
 */
ProblemInstance loadProblem(const std::string& filename);

/**
 * @brief Appends the summary of results from a vector of Bags
 * to a CSV file, including Seed, Local Search, and Metaheuristic parameters.
 *
 * @param bag The bag pointer containing solution data.
 * @param outputDir The directory to save the file in.
 * @param inputFilename The name of the original problem file (for logging).
 * @param fileId The id of the file to save in, used to differentiate multiple runs.
 */
void saveData(const std::unique_ptr<Bag>& bag,
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
 * @param timestamp The timestamp when the experiment was run.
 * @param outputDir The directory to save the report file in.
 * @param inputFilename The name of the original problem file.
 * @param fileId The id of the file based on loop index.
 * @returns Path to the saved report file.
 */
std::string saveReport(const std::unique_ptr<Bag>& bag,
                       const std::vector<Package*>& allPackages,
                       const std::vector<Dependency*>& allDependencies,
                       const std::string& timestamp,
                       const std::string& outputDir,
                       const std::string& inputFilename,
                       const std::string& fileId);

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
 * @return String describing the validation result.
 */
ValidationResult validateSolution(const std::string& problemFilename,
                                  const std::string& reportFilename);

/**
 * @brief Converts a path string using backslashes (\) to one
 * using forward slashes (/).
 */
std::string backslashesPathToSlashesPath(const std::string& s);

/**
 * @brief Formats a standard timestamp string (e.g., "YYYY-MM-DD HH:MM:SS")
 * into a string safe for use in a filename (e.g., "YYYY-MM-DD_HH-MM-SS").
 */
std::string formatTimestampForFileName(const std::string& ts);

/**
 * @brief Creates a unique output directory. If 'base' exists,
 * it tries 'base-1', 'base-2', etc., until a new directory can be created.
 *
 * @param base The desired base name for the directory.
 * @return The path of the successfully created directory.
 */
std::string createUniqueOutputDir(const std::string& base);

} // namespace FILE_PROCESSOR

#endif // FILEPROCESSOR_H