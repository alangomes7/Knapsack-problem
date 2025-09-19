#include "dependency.h"
#include "package.h"
#include <numeric>        // Required for std::accumulate (used in getTotalBenefit)

/**
 * Constructs a Dependency object with a name and a size.
 * The map of associated packages is initialized to be empty.
 */
Dependency::Dependency(const std::string& name, int size)
    : m_name(name),
      m_size(size) {
    // The m_associatedPackages map is automatically default-initialized.
}

const std::string& Dependency::getName() const {
    return m_name;
}

int Dependency::getSize() const {
    return m_size;
}

int Dependency::getTotalBenefit() const {
    // Use std::accumulate to sum the benefits of all packages in the map.
    // The initial sum is 0. The lambda function is called for each element.
    return std::accumulate(m_associatedPackages.begin(), m_associatedPackages.end(), 0,
        [](int current_sum, const auto& map_pair) {
            const Package* pkg = map_pair.second;
            return current_sum + (pkg ? pkg->getBenefit() : 0);
        });
}

std::unordered_map<std::string, Package*>& Dependency::getAssociatedPackages() {
    return m_associatedPackages;
}

const std::unordered_map<std::string, Package*>& Dependency::getAssociatedPackages() const {
    return m_associatedPackages;
}

void Dependency::addAssociatedPackage(Package* package) {
    if (package) {
        m_associatedPackages[package->getName()] = package;
    }
}

std::string Dependency::toString() const {
    return "Dependency(Name: '" + m_name +
           "', Size: " + std::to_string(m_size) +
           ", Total Benefit: " + std::to_string(getTotalBenefit()) +
           ", Associated Packages: " + std::to_string(m_associatedPackages.size()) + ")";
}