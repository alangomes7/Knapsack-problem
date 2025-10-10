#ifndef BAG_H
#define BAG_H

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "algorithm.h"

class Package;
class Dependency;

class Bag {
public:
    explicit Bag(Algorithm::ALGORITHM_TYPE bagAlgorithm, const std::string& timestamp);
    explicit Bag(const std::vector<Package*>& packages);

    const std::unordered_set<const Package*>& getPackages() const;
    const std::unordered_set<const Dependency*>& getDependencies() const;
    int getSize() const;
    int getBenefit() const;
    Algorithm::ALGORITHM_TYPE getBagAlgorithm() const;
    Algorithm::LOCAL_SEARCH getBagLocalSearch() const;
    double getAlgorithmTime() const;
    std::string getTimestamp() const;
    const std::string& getMetaheuristicParameters() const; // New getter

    void setAlgorithmTime(double seconds);
    void setLocalSearch(Algorithm::LOCAL_SEARCH localSearch);
    void setBagAlgorithm(Algorithm::ALGORITHM_TYPE bagAlgorithm);
    void setMetaheuristicParameters(const std::string& params); // New setter

    bool addPackage(const Package& package);
    void removePackage(const Package& package);
    bool canAddPackage(const Package& package, int maxCapacity) const;
    bool canSwap(const Package& packageIn, const Package& packageOut, int bagSize) const;
    std::string toString() const;

private:
    Algorithm::ALGORITHM_TYPE m_bagAlgorithm;
    Algorithm::LOCAL_SEARCH m_localSearch;
    std::string m_timeStamp = "0000-00-00 00:00:00";
    int m_size;
    int m_benefit;
    double m_algorithmTimeSeconds;
    std::string m_metaheuristicParams; // New member

    std::unordered_set<const Package*> m_baggedPackages;
    std::unordered_set<const Dependency*> m_baggedDependencies;
    std::unordered_map<const Dependency*, int> m_dependencyRefCount;
};

#endif // BAG_H