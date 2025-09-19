#ifndef DEPENDENCY_H
#define DEPENDENCY_H

#include <string>
#include <unordered_map>

// Forward declaration of the Package class to avoid circular header dependencies.
// The full definition of Package (Package.h) should be included in the corresponding
// implementation file (Dependency.cpp).
class Package;

/**
 * @brief Represents a dependency required by a system.
 *
 * A Dependency has a name and a size, and it is associated with a collection
 * of software packages that can fulfill it. This class tracks these associated
 * packages and can calculate their combined benefit.
 */
class Dependency {
public:
    /**
     * @brief Constructs a new Dependency object.
     * @param name The name of the dependency (e.g., "lib-ssl").
     * @param size The size requirement for the dependency (e.g., in MB).
     */
    explicit Dependency(const std::string& name, int size);

    /**
     * @brief Gets the name of the dependency.
     * @return A constant reference to the dependency's name.
     */
    const std::string& getName() const;

    /**
     * @brief Gets the size requirement of the dependency.
     * @return The integer size of the dependency.
     */
    int getSize() const;

    /**
     * @brief Calculates the total benefit of all packages associated with this dependency.
     *
     * This method iterates through all associated packages and sums their
     * individual benefit values.
     * @return The calculated total benefit as an integer.
     */
    int getTotalBenefit() const;

    /**
     * @brief Gets a mutable reference to the map of associated packages.
     *
     * This allows for direct modification of the associated packages map.
     * The map's key is the package ID, and the value is a raw pointer to the Package.
     * @return A reference to the map of package IDs to Package pointers.
     */
    std::unordered_map<std::string, Package*>& getAssociatedPackages();

    /**
     * @brief Gets a constant reference to the map of associated packages for read-only access.
     * @return A constant reference to the map of package IDs to Package pointers.
     */
    const std::unordered_map<std::string, Package*>& getAssociatedPackages() const;

    /**
     * @brief Associates a package with this dependency.
     *
     * Adds a pointer to a Package object to the internal map, using the package's
     * ID as the key. This requires the Package class to have a `getId()` method.
     *
     * @note This class does NOT take ownership of the Package pointer.
     * The caller is responsible for managing the Package object's lifetime.
     *
     * @param package A raw pointer to the Package object to be added. Must not be nullptr.
     */
    void addAssociatedPackage(Package* package);

    /**
     * @brief Creates a string representation of the Dependency object.
     *
     * Useful for logging or debugging purposes.
     * @return A std::string describing the dependency's current state.
     */
    std::string toString() const;

private:
    std::string m_name;
    int m_size;

    /**
     * @brief A map of associated packages, keyed by package ID.
     *
     * @note The Dependency class does not own these pointers. Memory management
     * for the Package objects must be handled elsewhere.
     */
    std::unordered_map<std::string, Package*> m_associatedPackages;
};

#endif // DEPENDENCY_H