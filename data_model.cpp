#include "data_model.h"
#include "package.h"    // Need full definitions for deep copy
#include "dependency.h" // Need full definitions for deep copy
#include <unordered_map>

// --- Implementation of ProblemInstance Memory Management ---

/**
 * @brief Helper function to delete all heap-allocated objects.
 */
void ProblemInstance::clear() {
    // Delete all packages
    for (Package* pkg : packages) {
        delete pkg;
    }
    packages.clear();

    // Delete all dependencies
    for (Dependency* dep : dependencies) {
        delete dep;
    }
    dependencies.clear();
}

/**
 * @brief Destructor implementation.
 */
ProblemInstance::~ProblemInstance() {
    clear();
}

/**
 * @brief Copy Constructor (Deep Copy) implementation.
 */
ProblemInstance::ProblemInstance(const ProblemInstance& other) : maxCapacity(other.maxCapacity) {
    
    // 1. Create maps to link old pointers to new copies
    std::unordered_map<const Dependency*, Dependency*> oldToNewDeps;
    std::unordered_map<const Package*, Package*> oldToNewPkgs;

    // 2. First Pass: Deep copy all Dependency objects
    //    (At this point, their internal Package maps still point to old packages)
    dependencies.reserve(other.dependencies.size());
    for (const Dependency* oldDep : other.dependencies) {
        Dependency* newDep = new Dependency(*oldDep); // Uses default copy ctor
        dependencies.push_back(newDep);
        oldToNewDeps[oldDep] = newDep; // Map old pointer to new one
    }

    // 3. First Pass: Deep copy all Package objects
    //    (At this point, their internal Dependency maps still point to old dependencies)
    packages.reserve(other.packages.size());
    for (const Package* oldPkg : other.packages) {
        Package* newPkg = new Package(*oldPkg); // Uses default copy ctor
        packages.push_back(newPkg);
        oldToNewPkgs[oldPkg] = newPkg; // Map old pointer to new one
    }

    // 4. Second Pass: "Re-wire" pointers in new Packages
    for (Package* newPkg : packages) {
        // Get the dependency map (which has old pointers)
        auto& oldDepsMap = newPkg->getDependencies();
        
        // Create a new map to hold pointers to the new dependencies
        std::unordered_map<std::string, Dependency*> newDepsMap;
        for (const auto& pair : oldDepsMap) {
            Dependency* oldDep = pair.second;
            Dependency* newDep = oldToNewDeps[oldDep]; // Find the new pointer
            newDepsMap[pair.first] = newDep;
        }
        
        // Replace the old map with the re-wired map
        oldDepsMap = std::move(newDepsMap);
    }

    // 5. Second Pass: "Re-wire" pointers in new Dependencies
    for (Dependency* newDep : dependencies) {
        // Get the associated package map (which has old pointers)
        auto& oldPkgsMap = newDep->getAssociatedPackages();
        
        // Create a new map to hold pointers to the new packages
        std::unordered_map<std::string, Package*> newPkgsMap;
        for (const auto& pair : oldPkgsMap) {
            Package* oldPkg = pair.second;
            Package* newPkg = oldToNewPkgs[oldPkg]; // Find the new pointer
            newPkgsMap[pair.first] = newPkg;
        }

        // Replace the old map with the re-wired map
        oldPkgsMap = std::move(newPkgsMap);
    }
}

/**
 * @brief Copy Assignment Operator implementation.
 */
ProblemInstance& ProblemInstance::operator=(const ProblemInstance& other) {
    // 1. Check for self-assignment
    if (this == &other) {
        return *this;
    }

    // 2. Clear existing data
    clear();
    
    // 3. Use the copy constructor logic to copy 'other' into 'this'
    //    We create a temporary deep copy, then swap its contents with ours.
    ProblemInstance temp(other);
    
    // Swap the data members
    std::swap(maxCapacity, temp.maxCapacity);
    std::swap(packages, temp.packages);
    std::swap(dependencies, temp.dependencies);
    
    // 4. When 'temp' goes out of scope, its destructor
    //    will clean up the data we *used* to own.
    return *this;
}