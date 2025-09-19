#ifndef PACKAGE_H
#define PACKAGE_H

#include <string>
#include <unordered_map>

class Dependency;

/**
 * @brief Represents a software package with a benefit value and dependencies.
 *
 * A Package has a unique name, an intrinsic "benefit" value, and a collection
 * of dependencies it requires to function. 📦
 */
class Package {
public:
    /**
     * @brief Constructs a new Package object.
     * @param name The unique name of the package (e.g., "openssl").
     * @param benefit The intrinsic benefit value provided by this package.
     */
    explicit Package(const std::string& name, int benefit);

    /**
     * @brief Gets the unique name of the package, which serves as its ID.
     * @return A constant reference to the package's name.
     */
    const std::string& getName() const;

    /**
     * @brief Gets the intrinsic benefit value of installing this package.
     * @return The integer benefit value.
     */
    int getBenefit() const;

    /**
     * @brief Calculates the total size of all package dependencies.
     *
     * Iterates through all dependencies and sums their individual sizes to
     * compute the total disk space required. This is a read-only operation.
     *
     * @return The total size of all dependencies in MB.
     */
    int getDependenciesSize() const;

    /**
     * @brief Gets a mutable reference to the map of this package's dependencies.
     *
     * This allows for direct modification of the dependencies. The map's key
     * is the dependency's name.
     * @return A reference to the map of dependency names to Dependency pointers.
     */
    std::unordered_map<std::string, Dependency*>& getDependencies();

    /**
     * @brief Gets a constant reference to the map of dependencies for read-only access.
     * @return A constant reference to the map of dependency names to Dependency pointers.
     */
    const std::unordered_map<std::string, Dependency*>& getDependencies() const;

    /**
     * @brief Adds a dependency requirement to this package.
     *
     * This method stores a raw pointer to the provided Dependency object, using
     * the dependency's name as the key for the internal map.
     *
     * @warning The Package class does **NOT** take ownership of the Dependency pointer.
     * The caller is responsible for ensuring the Dependency object remains valid
     * for the entire lifetime of this Package, otherwise the stored pointer will
     * become a "dangling pointer," leading to undefined behavior. ⚠️
     *
     * @param dependency A reference to the Dependency object to add.
     */
    void addDependency(Dependency& dependency);

    /**
     * @brief Creates a string representation of the Package object.
     *  Generates a detailed, human-readable string representation of the
     * package, including its name, benefit, and a list of its dependencies.
     *
     * Useful for logging or debugging purposes.
     * @return A std::string describing the package's current state.
     */
    std::string toString() const;

private:
    std::string m_name;
    int m_benefit;

    /**
     * @brief A map of dependencies required by this package, keyed by dependency name.
     *
     * @note The Package class does not own these pointers. Memory management
     * for the Dependency objects must be handled externally.
     */
    std::unordered_map<std::string, Dependency*> m_dependencies;
};

#endif // PACKAGE_H