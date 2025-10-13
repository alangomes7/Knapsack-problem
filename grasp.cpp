#include "grasp.h"

#include "bag.h"
#include "package.h"
#include "dependency.h"
#include "algorithm.h"

#include <chrono>
#include <limits>
#include <algorithm>
#include <iterator>
#include <numeric>
#include <cmath>

GRASP::GRASP(double maxTime)
    : m_maxTime(maxTime), m_generator(std::random_device{}())
{
    // default reactive alphas - wider range for better exploration
    m_alphas = {0.0, 0.15, 0.3, 0.5, 0.7, 0.75, 8, 8.5};
    m_alphaProbs.assign(m_alphas.size(), 1.0 / m_alphas.size());
    m_alphaScores.assign(m_alphas.size(), 0.0);
    m_alphaCounts.assign(m_alphas.size(), 0);
    m_ilsRounds = 0;
    m_perturbStrength = 0;
    m_eliteSetSize = 5;
    m_usePathRelinking = true;
}

GRASP::GRASP(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_generator(seed), m_helper(seed)
{
    m_alphas = {0.0, 0.15, 0.3, 0.5, 0.7};
    m_alphaProbs.assign(m_alphas.size(), 1.0 / m_alphas.size());
    m_alphaScores.assign(m_alphas.size(), 0.0);
    m_alphaCounts.assign(m_alphas.size(), 0);
    m_ilsRounds = 0;
    m_perturbStrength = 0;
    m_eliteSetSize = 5;
    m_usePathRelinking = true;
}

GRASP::GRASP(double maxTime, unsigned int seed,
             int ilsRounds, int perturbStrength,
             const std::vector<double>& reactiveAlphas)
    : m_maxTime(maxTime), m_generator(seed), m_helper(seed),
      m_ilsRounds(ilsRounds), m_perturbStrength(perturbStrength),
      m_alphas(reactiveAlphas)
{
    if (m_alphas.empty()) {
        m_alphas = {0.0, 0.15, 0.3, 0.5, 0.7};
    }
    m_alphaProbs.assign(m_alphas.size(), 1.0 / m_alphas.size());
    m_alphaScores.assign(m_alphas.size(), 0.0);
    m_alphaCounts.assign(m_alphas.size(), 0);
    m_eliteSetSize = 5;
    m_usePathRelinking = true;
}

Bag* GRASP::run(int bagSize, const std::vector<Bag*>& initialBags,
                const std::vector<Package*>& allPackages,
                Algorithm::LOCAL_SEARCH localSearchMethod,
                const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    using clock = std::chrono::high_resolution_clock;
    auto start = clock::now();
    std::chrono::duration<double> maxDur(m_maxTime);

    Bag* bestBag = nullptr;
    int bestBenefit = std::numeric_limits<int>::min();
    
    // Elite set for path relinking
    std::vector<Bag*> eliteSet;

    // Initialize best from initialBags if any
    if (!initialBags.empty()) {
        bestBag = new Bag(*initialBags.front());
        m_helper.makeItFeasible(*bestBag, bagSize, dependencyGraph);
        bestBenefit = bestBag->getBenefit();
        // Add initial bags to elite set
        for (auto* bag : initialBags) {
            if (eliteSet.size() < m_eliteSetSize) {
                eliteSet.push_back(new Bag(*bag));
            }
        }
    } else {
        bestBag = new Bag(Algorithm::ALGORITHM_TYPE::GRASP, "0");
    }

    // ILS params
    if (m_ilsRounds == 0){
        m_ilsRounds = std::max(3, static_cast<int>(allPackages.size()));
        m_perturbStrength = std::max(2, static_cast<int>(allPackages.size() * 1.75));
    }

    int iterationsSinceImprovement = 0;
    int totalIterations = 0;
    const int maxNoImprovement = 10; // Trigger intensification after this many iterations

    // Loop while time remains
    while (clock::now() - start < maxDur) {
        // 1 Select starting solution with diversification strategy
        static size_t initIndex = 0;
        Bag* candidate = nullptr;
        double usedAlpha = 0.0;
        bool fromInitial = false;

        if (!initialBags.empty() && initIndex < initialBags.size()) {
            candidate = new Bag(*initialBags[initIndex]);
            fromInitial = true;
            ++initIndex;
        } else {
            // Adaptive strategy: use higher alpha for diversification when stuck
            if (iterationsSinceImprovement > maxNoImprovement / 2) {
                // Force exploration with higher randomness
                usedAlpha = m_alphas.back(); // Use highest alpha
            } else {
                usedAlpha = pickAlpha();
            }
            
            candidate = construction(bagSize, allPackages, dependencyGraph, usedAlpha);
            
            // Dynamic perturbation strength
            double perturbRate = 0.3 + (iterationsSinceImprovement * 0.05);
            perturbRate = std::min(0.8, perturbRate);
            m_perturbStrength = std::max(2, static_cast<int>(allPackages.size() * perturbRate));
        }

        int beforeLS = candidate->getBenefit();

        // 2 Apply Local Search with adaptive intensity
        int lsIterations = (iterationsSinceImprovement > maxNoImprovement) ? 
                          m_ilsRounds * 1.3 : m_ilsRounds; // Intensification
        m_localSearch.run(*candidate, bagSize, allPackages, localSearchMethod, dependencyGraph, lsIterations);

        // 3 Apply ILS improvement phase with adaptive perturbation
        Bag* current = candidate;
        for (int r = 0; r < m_ilsRounds; ++r) {
            // Variable perturbation strength within ILS
            int adaptivePerturbStrength = m_perturbStrength + m_helper.randomNumberInt(-20, 20);
            adaptivePerturbStrength = std::max(1, std::min(adaptivePerturbStrength, static_cast<int>(allPackages.size() * 0.9)));
            
            Bag* perturbed = perturbSolution(*current, bagSize * 1.3, allPackages, dependencyGraph, adaptivePerturbStrength);
            m_localSearch.run(*perturbed, bagSize, allPackages, localSearchMethod, dependencyGraph, 300);

            if (perturbed->getBenefit() > current->getBenefit()) {
                delete current;
                current = perturbed;
            } else {
                delete perturbed;
            }

            if (clock::now() - start > maxDur)
                break;
        }

        // 4 Path Relinking with elite set
        if (m_usePathRelinking && !eliteSet.empty() && 
            (totalIterations % 5 == 0) && // Apply every 5 iterations
            (clock::now() - start < maxDur * 0.9)) { // Don't use in final phase
            
            Bag* prBag = pathRelink(*current, *eliteSet[m_helper.randomNumberInt(0, eliteSet.size() - 1)],
                                    bagSize, allPackages, dependencyGraph);
            if (prBag && prBag->getBenefit() > current->getBenefit()) {
                delete current;
                current = prBag;
            } else {
                delete prBag;
            }
        }

        // 5 Keep the best and update elite set
        bool improved = false;
        if (current->getBenefit() > bestBenefit) {
            delete bestBag;
            bestBag = current;
            bestBenefit = bestBag->getBenefit();
            improved = true;
            iterationsSinceImprovement = 0;
            
            // Add to elite set
            updateEliteSet(eliteSet, new Bag(*current));
        } else {
            // Consider adding good solutions to elite set even if not best
            if (current->getBenefit() >= bestBenefit * 0.95) { // Within 95% of best
                updateEliteSet(eliteSet, new Bag(*current));
            }
            delete current;
            iterationsSinceImprovement++;
        }

        // 6 Update reactive alpha statistics (FIX: use actual alpha)
        if (!fromInitial) {
            double improvement = static_cast<double>(current->getBenefit() - beforeLS);
            updateAlphaRewards(usedAlpha, improvement);
        }

        totalIterations++;

        // 7 Periodic alpha probability reset for continuous adaptation
        if (totalIterations % 50 == 0) {
            smoothAlphaProbs(0.1); // Add small uniform component
        }

        if (clock::now() - start > maxDur)
            break;
    }

    // Cleanup elite set
    for (auto* bag : eliteSet) {
        delete bag;
    }

    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::GRASP);
    bestBag->setLocalSearch(localSearchMethod);
    bestBag->setAlgorithmTime(std::chrono::duration<double>(clock::now() - start).count());
    return bestBag;
}

Bag* GRASP::construction(int bagSize, const std::vector<Package*>& allPackages,
                         const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                         double alpha)
{
    Bag* newBag = new Bag(Algorithm::ALGORITHM_TYPE::GRASP, "0");
    std::vector<Package*> candidates = allPackages;
    std::vector<std::pair<Package*, double>> scored;
    scored.reserve(candidates.size());
    std::vector<Package*> rcl;
    rcl.reserve(candidates.size());

    // Use multiple greedy criteria for more diverse construction
    int criteriaSwitch = m_helper.randomNumberInt(0, 2);

    while (!candidates.empty()) {
        scored.clear();
        for (Package* p : candidates) {
            const int depsSize = p->getDependenciesSize();
            if (depsSize <= 0) continue;
            
            double score;
            switch (criteriaSwitch) {
                case 0: // Benefit per dependency (original)
                    score = static_cast<double>(p->getBenefit()) / depsSize;
                    break;
                case 1: // Pure benefit (greedy)
                    score = static_cast<double>(p->getBenefit());
                    break;
                case 2: // Benefit per sqrt(dependency) - balanced
                    score = static_cast<double>(p->getBenefit()) / std::sqrt(depsSize);
                    break;
                default:
                    score = static_cast<double>(p->getBenefit()) / depsSize;
            }
            scored.emplace_back(p, score);
        }
        if (scored.empty()) break;

        auto [minIt, maxIt] = std::minmax_element(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        const double bestScore = maxIt->second;
        const double worstScore = minIt->second;
        const double threshold = bestScore - alpha * (bestScore - worstScore);

        // RCL creation
        rcl.clear();
        for (auto& [pkg, score] : scored) {
            if (score >= threshold) rcl.push_back(pkg);
        }

        if (rcl.empty()) break;
        Package* selected = rcl[m_helper.randomNumberInt(0, (int)rcl.size() - 1)];

        const auto it = dependencyGraph.find(selected);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};

        if (newBag->canAddPackage(*selected, bagSize, deps)) {
            newBag->addPackage(*selected, deps);
        }

        candidates.erase(std::remove(candidates.begin(), candidates.end(), selected), candidates.end());
    }

    return newBag;
}

Bag* GRASP::perturbSolution(const Bag& source, int bagSize,
                            const std::vector<Package*>& allPackages,
                            const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                            int removeCount)
{
    Bag* pert = new Bag(source);
    std::vector<const Package*> inBagVec(pert->getPackages().begin(), pert->getPackages().end());
    if (inBagVec.empty()) return pert;

    removeCount = std::min(removeCount, static_cast<int>(inBagVec.size()));

    // Strategy: remove mix of low-benefit and random packages
    int targetRemove = removeCount;
    int lowBenefitRemove = targetRemove / 2;
    int randomRemove = targetRemove - lowBenefitRemove;

    // Sort by benefit to identify low-benefit packages
    std::vector<const Package*> sortedByBenefit = inBagVec;
    std::sort(sortedByBenefit.begin(), sortedByBenefit.end(),
              [](const Package* a, const Package* b) { return a->getBenefit() < b->getBenefit(); });

    // Remove low-benefit packages
    for (int i = 0; i < lowBenefitRemove && i < sortedByBenefit.size(); ++i) {
        const Package* p = sortedByBenefit[i];
        auto it = dependencyGraph.find(p);
        const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? 
                                                     it->second : std::vector<const Dependency*>{};
        pert->removePackage(*p, deps);
        inBagVec.erase(std::remove(inBagVec.begin(), inBagVec.end(), p), inBagVec.end());
    }

    // Remove random packages
    for (int i = 0; i < randomRemove && !inBagVec.empty(); ++i) {
        int idx = m_helper.randomNumberInt(0, (int)inBagVec.size() - 1);
        const Package* p = inBagVec[idx];
        auto it = dependencyGraph.find(p);
        const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? 
                                                     it->second : std::vector<const Dependency*>{};
        pert->removePackage(*p, deps);
        std::swap(inBagVec[idx], inBagVec.back());
        inBagVec.pop_back();
    }

    // Reconstruction with variable alpha for diversity
    double reconAlpha = 0.1 + m_helper.randomNumberInt(0, 30) / 100.0; // 0.1 to 0.4
    
    std::vector<Package*> candidates;
    candidates.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (pert->getPackages().count(p) == 0) candidates.push_back(p);
    }

    while (!candidates.empty()) {
        std::vector<std::pair<Package*, double>> scored;
        for (Package* p : candidates) {
            if (p->getDependenciesSize() <= 0) continue;
            scored.emplace_back(p, static_cast<double>(p->getBenefit()) / p->getDependenciesSize());
        }
        if (scored.empty()) break;
        
        auto [minIt, maxIt] = std::minmax_element(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        double best = maxIt->second;
        double worst = minIt->second;
        double threshold = best - reconAlpha * (best - worst);

        std::vector<Package*> rcl;
        for (auto &pr : scored) {
            if (pr.second >= threshold) rcl.push_back(pr.first);
        }
        if (rcl.empty()) break;
        
        Package* pick = rcl[m_helper.randomNumberInt(0, (int)rcl.size() - 1)];
        auto it = dependencyGraph.find(pick);
        const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? 
                                                     it->second : std::vector<const Dependency*>{};
        if (pert->canAddPackage(*pick, bagSize, deps)) {
            pert->addPackage(*pick, deps);
        }

        candidates.erase(std::remove(candidates.begin(), candidates.end(), pick), candidates.end());
    }

    return pert;
}

Bag* GRASP::pathRelink(const Bag& source, const Bag& target, int bagSize,
                       const std::vector<Package*>& allPackages,
                       const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    Bag* current = new Bag(source);
    Bag* best = new Bag(source);
    
    // Identify differences
    std::vector<const Package*> toAdd, toRemove;
    
    for (const Package* p : target.getPackages()) {
        if (current->getPackages().count(p) == 0) {
            toAdd.push_back(p);
        }
    }
    
    for (const Package* p : current->getPackages()) {
        if (target.getPackages().count(p) == 0) {
            toRemove.push_back(p);
        }
    }
    
    // Path relinking: gradually transform source to target
    size_t steps = std::max(toAdd.size(), toRemove.size());
    for (size_t step = 0; step < steps; ++step) {
        // Remove one package
        if (step < toRemove.size()) {
            const Package* p = toRemove[step];
            auto it = dependencyGraph.find(p);
            const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? 
                                                         it->second : std::vector<const Dependency*>{};
            current->removePackage(*p, deps);
        }
        
        // Add one package
        if (step < toAdd.size()) {
            const Package* p = toAdd[step];
            auto it = dependencyGraph.find(p);
            const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? 
                                                         it->second : std::vector<const Dependency*>{};
            if (current->canAddPackage(*p, bagSize, deps)) {
                current->addPackage(*p, deps);
            }
        }
        
        // Track best solution along path
        if (current->getBenefit() > best->getBenefit()) {
            delete best;
            best = new Bag(*current);
        }
    }
    
    delete current;
    return best;
}

void GRASP::updateEliteSet(std::vector<Bag*>& eliteSet, Bag* candidate)
{
    // Check if candidate is already in elite set (avoid duplicates)
    for (const auto* bag : eliteSet) {
        if (bag->getBenefit() == candidate->getBenefit()) {
            delete candidate;
            return;
        }
    }
    
    // If elite set not full, add directly
    if (eliteSet.size() < m_eliteSetSize) {
        eliteSet.push_back(candidate);
        return;
    }
    
    // Find worst in elite set
    auto worstIt = std::min_element(eliteSet.begin(), eliteSet.end(),
        [](const Bag* a, const Bag* b) { return a->getBenefit() < b->getBenefit(); });
    
    // Replace if candidate is better
    if (candidate->getBenefit() > (*worstIt)->getBenefit()) {
        delete *worstIt;
        *worstIt = candidate;
    } else {
        delete candidate;
    }
}

double GRASP::pickAlpha()
{
    std::discrete_distribution<int> dist(m_alphaProbs.begin(), m_alphaProbs.end());
    int idx = dist(m_generator);
    m_alphaCounts[idx] += 1;
    return m_alphas[idx];
}

void GRASP::updateAlphaRewards(double alpha, double improvement)
{
    auto it = std::find(m_alphas.begin(), m_alphas.end(), alpha);
    if (it == m_alphas.end()) return;
    int idx = static_cast<int>(std::distance(m_alphas.begin(), it));
    
    // Reward positive improvements more strongly
    double reward = std::max(0.0, improvement);
    m_alphaScores[idx] += reward;

    // Recompute probabilities with better normalization
    std::vector<double> values(m_alphas.size(), 0.0);
    double total = 0.0;
    for (size_t i = 0; i < m_alphas.size(); ++i) {
        if (m_alphaCounts[i] > 0) {
            values[i] = m_alphaScores[i] / static_cast<double>(m_alphaCounts[i]);
        }
        values[i] = values[i] + 1e-4; // Small floor
        total += values[i];
    }
    
    if (total > 0.0) {
        for (size_t i = 0; i < m_alphaProbs.size(); ++i) {
            m_alphaProbs[i] = values[i] / total;
        }
    } else {
        double u = 1.0 / static_cast<double>(m_alphaProbs.size());
        for (auto &p : m_alphaProbs) p = u;
    }
}

void GRASP::smoothAlphaProbs(double uniformWeight)
{
    // Mix current probabilities with uniform distribution to avoid premature convergence
    double uniform = 1.0 / m_alphas.size();
    for (size_t i = 0; i < m_alphaProbs.size(); ++i) {
        m_alphaProbs[i] = (1.0 - uniformWeight) * m_alphaProbs[i] + uniformWeight * uniform;
    }
}