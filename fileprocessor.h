#ifndef FILEPROCESSOR_H
#define FILEPROCESSOR_H

#include <string>
#include <vector>

class Package;
class Dependency;
class Bag;

class FileProcessor
{
public:
    
    /**
     * @brief Constructs a FileProcessor object and reads the specified file.
     * @param filePath The path to the input file.
     */
    FileProcessor(const std::string& filePath);

    const std::string getPath() const;

    /**
     * @brief Gets the data read from the file.
     * @return A const reference to a vector of vectors of strings.
     */
    const std::vector<std::vector<std::string>>& getProcessedData() const;

    /**
     * @brief Saves the results of the algorithm to a CSV file.
     * @param bags A vector of Bag pointers representing the solutions.
     */
    void saveData(const std::vector<Bag*>& bags);

    /**
     * @brief Saves a detailed report of the best solution found.
     * @param bags A vector of Bag pointers, representing all the solutions found.
     * @param allPackages A vector containing pointers to all possible packages.
     * @param allDependencies A vector containing pointers to all possible dependencies.
     * @param seed The random seed used for the experiment.
     * @param timestamp The timestamp of when the experiment was run.
     */
    void saveReport(const std::vector<Bag*>& bags,
                                const std::vector<Package*>& allPackages,
                                const std::vector<Dependency*>& allDependencies,
                                unsigned int seed, const std::string& timestamp);

    std::string backslashesPathToSlashesPath(std::string backslashesPath);

    /**
     * @brief Formats a timestamp string to be safe for use in a filename.
     * @param timestamp The timestamp string to format.
     * @return A filesystem-safe string.
     */
    std::string formatTimestampForFilename(std::string timestamp);


private:
    std::vector<std::vector<std::string>> m_processedData;
    std::string fileinputPath;
    std::string m_output_dir;

    /**
     * @brief Reads and parses the file specified in the constructor.
     */
    void readFile();
};

#endif // FILEPROCESSOR_H