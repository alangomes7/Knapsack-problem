#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

#include <string>
#include <vector>
#include <map>

#include "DataStructures.h"

// Forward declaration for Bag (still needed here)
class Bag;
// Package and Dependency are now forward-declared in DataStructures.h

/**
 * @brief Contains utilities for loading problem files, loading solution reports,
 * and saving new experiment results and reports to disk.
 */
namespace FileProcessor {

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
 */
void saveData(const std::vector<Bag*>& bags,
              const std::string& outputDir,
              const std::string& inputFilename);

/**
 * @brief Saves a detailed .txt report for the best solution found,
 * which can be used for reproduction and validation.
 *
 * @param bags Vector of all solution bags from the experiment.
 * @param allPackages Vector of all possible packages in the problem.
 * @param allDependencies Vector of all possible dependencies.
 * @param seed The random seed used for the experiment.
 * @param timestamp The timestamp when the experiment was run.
 * @param outputDir The directory to save the report file in.
 * @param inputFilename The name of the original problem file.
 */
void saveReport(const std::vector<Bag*>& bags,
                const std::vector<Package*>& allPackages,
                const std::vector<Dependency*>& allDependencies,
                unsigned int seed,
                const std::string& timestamp,
                const std::string& outputDir,
                const std::string& inputFilename);

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
std::string formatTimestampForFilename(const std::string& ts);

/**
 * @brief Creates a unique output directory. If 'base' exists,
 * it tries 'base-1', 'base-2', etc., until a new
 * directory can be created.
 *
 * @param base The desired base name for the directory.
 * @return The path of the successfully created directory.
 */
std::string createUniqueOutputDir(const std::string& base);

/**
 * @brief Loads a previously generated solution report file for validation.
 *
 * @param filename The path to the report file.
 * @return A SolutionReport struct containing the data from the report.
 * @throws std::runtime_error if the file cannot be opened.
 */
SolutionReport loadReport(const std::string& filename);

} // namespace FileProcessor

#endif // FILEPROCESSOR_H