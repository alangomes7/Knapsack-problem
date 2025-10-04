#include "package.h"
#include "dependency.h"

Package::Package(const std::string& name, int benefit)
    : m_name(name),
      m_benefit(benefit) {
    // No explicit body needed as members are initialized in the initializer list.
}

const std::string& Package::getName() const {
    return m_name;
}

int Package::getBenefit() const {
    return m_benefit;
}

int Package::getDependenciesSize() const
{
    int dependenciesSize = 0;
    
    for (const auto& pair : m_dependencies) {
        dependenciesSize += pair.second->getSize();
    }
    return dependenciesSize;
}

std::unordered_map<std::string, Dependency*>& Package::getDependencies() {
    return m_dependencies;
}

const std::unordered_map<std::string, Dependency*>& Package::getDependencies() const {
    return m_dependencies;
}

void Package::addDependency(Dependency& dependency) {
    m_dependencies[dependency.getName()] = &dependency;
}

bool Package::hasDependency(const Dependency *dependency) const
{
    if (!dependency) {
        return false;
    }
    return m_dependencies.count(dependency->getName()) > 0;
}

std::string Package::toString() const
{
    std::string package_string;
    package_string += "Package: " + m_name + "\n";
    package_string += "Benefit: " + std::to_string(m_benefit) + "\n";
    package_string += "Dependencies (" + std::to_string(m_dependencies.size()) + "):\n";

    if (m_dependencies.empty()) {
        package_string += "  None\n";
    } else {
        // Iterate through the map to list each dependency.
        for (const auto& pair : m_dependencies) {
            const Dependency* dependency = pair.second;
            // Good practice to check if the pointer is valid.
            if (dependency) {
                package_string += "  - " + dependency->getName() +
                                  " (Size: " + std::to_string(dependency->getSize()) + ")\n";
            }
        }
    }
    return package_string;
}