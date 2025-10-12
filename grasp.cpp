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
    // default reactive alphas
    m_alphas = {0.0, 0.1, 0.2, 0.3, 0.5};
    m_alphaProbs.assign(m_alphas.size(), 1.0 / m_alphas.size());
    m_alphaScores.assign(m_alphas.size(), 0.0);
    m_alphaCounts.assign(m_alphas.size(), 0);
}

GRASP::GRASP(double maxTime, unsigned int seed)
    : m_maxTime(maxTime), m_generator(seed)
{
    m_alphas = {0.0, 0.1, 0.2, 0.3, 0.5};
    m_alphaProbs.assign(m_alphas.size(), 1.0 / m_alphas.size());
    m_alphaScores.assign(m_alphas.size(), 0.0);
    m_alphaCounts.assign(m_alphas.size(), 0);
}

GRASP::GRASP(double maxTime, unsigned int seed,
             int ilsRounds, int perturbStrength,
             const std::vector<double>& reactiveAlphas)
    : m_maxTime(maxTime), m_generator(seed),
      m_ilsRounds(ilsRounds), m_perturbStrength(perturbStrength),
      m_alphas(reactiveAlphas)
{
    if (m_alphas.empty()) {
        m_alphas = {0.0, 0.1, 0.2, 0.3, 0.5};
    }
    m_alphaProbs.assign(m_alphas.size(), 1.0 / m_alphas.size());
    m_alphaScores.assign(m_alphas.size(), 0.0);
    m_alphaCounts.assign(m_alphas.size(), 0);
}

/**
 * Main run loop:
 * - While time remains:
 *     - pick alpha (reactive)
 *     - construct solution with alpha
 *     - local search
 *     - run ILS (perturb + local search several rounds)
 *     - update best solution
 *     - update reactive alpha rewards based on improvement produced by this alpha
 */
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

    // Initialize best from initialBags if any
    if (!initialBags.empty()) {
        bestBag = new Bag(*initialBags.front());
        bestBenefit = bestBag->getBenefit();
    } else {
        bestBag = new Bag(Algorithm::ALGORITHM_TYPE::GRASP, "0");
    }

    // Loop while time remains
    while (clock::now() - start < maxDur) {
        // 1️⃣ Select starting solution:
        //    Cycle through initialBags first, then random constructions afterward.
        static size_t initIndex = 0;
        Bag* candidate = nullptr;
        bool fromInitial = false;

        if (!initialBags.empty() && initIndex < initialBags.size()) {
            candidate = new Bag(*initialBags[initIndex]);
            fromInitial = true;
            ++initIndex;
        } else {
            double alpha = pickAlpha();
            candidate = construction(bagSize, allPackages, dependencyGraph, alpha);
        }

        int beforeLS = candidate->getBenefit();

        // 2️⃣ Apply Local Search
        localSearch(*candidate, bagSize, allPackages, localSearchMethod, dependencyGraph);

        // 3️⃣ Apply ILS improvement phase
        Bag* current = candidate;
        for (int r = 0; r < m_ilsRounds; ++r) {
            Bag* perturbed = perturbSolution(*current, bagSize, allPackages, dependencyGraph, m_perturbStrength);
            localSearch(*perturbed, bagSize, allPackages, localSearchMethod, dependencyGraph);

            if (perturbed->getBenefit() > current->getBenefit()) {
                delete current;
                current = perturbed;
            } else {
                delete perturbed;
            }

            if (clock::now() - start > maxDur)
                break; // stop if time expired during ILS
        }

        // 4️⃣ Update reactive α statistics (only if construction was used)
        if (!fromInitial) {
            double improvement = static_cast<double>(current->getBenefit() - beforeLS);
            updateAlphaRewards(m_alphas.front(), improvement); // optional: use the alpha used for this iteration
        }

        // 5️⃣ Keep the best
        if (current->getBenefit() > bestBenefit) {
            delete bestBag;
            bestBag = current;
            bestBenefit = bestBag->getBenefit();
        } else {
            delete current;
        }

        if (clock::now() - start > maxDur)
            break;
    }

    bestBag->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::GRASP);
    bestBag->setLocalSearch(localSearchMethod);
    bestBag->setAlgorithmTime(std::chrono::duration<double>(clock::now() - start).count());
    return bestBag;
}

/**
 * Construction with given alpha: compute score = benefit/weight,
 * create RCL by threshold: score >= best - alpha*(best-worst),
 * pick randomly from RCL. Always use dependencyGraph.find() safely.
 */
/**
 * Faster construction: uses precomputed benefit/weight ratio and avoids full sorting.
 */
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

    while (!candidates.empty()) {
        scored.clear();
        for (Package* p : candidates) {
            const int depsSize = p->getDependenciesSize();
            if (depsSize <= 0) continue;
            scored.emplace_back(p, double(p->getBenefit()) / depsSize);
        }
        if (scored.empty()) break;

        auto [minIt, maxIt] = std::minmax_element(scored.begin(), scored.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        const double bestScore = maxIt->second;
        const double worstScore = minIt->second;
        const double threshold = bestScore - alpha * (bestScore - worstScore);

        // RCL creation faster: no sort, single scan
        rcl.clear();
        for (auto& [pkg, score] : scored) {
            if (score >= threshold) rcl.push_back(pkg);
        }

        if (rcl.empty()) break;
        Package* selected = rcl[randomNumberInt(0, (int)rcl.size() - 1)];

        const auto it = dependencyGraph.find(selected);
        const auto& deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};

        if (newBag->canAddPackage(*selected, bagSize, deps)) {
            newBag->addPackage(*selected, deps);
        }

        // remove selected
        candidates.erase(std::remove(candidates.begin(), candidates.end(), selected), candidates.end());
    }

    return newBag;
}

/**
 * Local search: repeated best-improvement swap neighborhood until local optimum.
 */
bool GRASP::localSearch(Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
                        Algorithm::LOCAL_SEARCH localSearchMethod,
                        const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    bool improvedAny = false;
    bool stop = false;
    while (!stop) {
        bool found = false;
        if (localSearchMethod == Algorithm::LOCAL_SEARCH::BEST_IMPROVEMENT) {
            found = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages, dependencyGraph);
        } else {
            // If other local search methods exist, add here.
            found = exploreSwapNeighborhoodBestImprovement(currentBag, bagSize, allPackages, dependencyGraph);
        }

        if (found) improvedAny = true;
        else stop = true;
    }
    return improvedAny;
}

/**
 * Faster swap exploration — precompute feasible pairs, reuse benefit diffs.
 */
bool GRASP::exploreSwapNeighborhoodBestImprovement(
    Bag& currentBag, int bagSize, const std::vector<Package*>& allPackages,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph)
{
    const auto& packagesInBag = currentBag.getPackages();
    if (packagesInBag.empty()) return false;

    std::vector<Package*> outside;
    outside.reserve(allPackages.size());
    for (Package* p : allPackages)
        if (packagesInBag.count(p) == 0) outside.push_back(p);

    int bestDelta = 0;
    const Package* bestIn = nullptr;
    Package* bestOut = nullptr;

    for (const Package* pIn : packagesInBag) {
        const int inBenefit = pIn->getBenefit();
        for (Package* pOut : outside) {
            const int delta = pOut->getBenefit() - inBenefit;
            if (delta <= bestDelta) continue; // skip if no chance of improvement
            if (!currentBag.canSwap(*pIn, *pOut, bagSize)) continue;
            bestDelta = delta;
            bestIn = pIn;
            bestOut = pOut;
        }
    }

    if (!bestIn || !bestOut) return false;

    const auto itIn = dependencyGraph.find(bestIn);
    const auto& depsIn = (itIn != dependencyGraph.end()) ? itIn->second : std::vector<const Dependency*>{};
    currentBag.removePackage(*bestIn, depsIn);

    const auto itOut = dependencyGraph.find(bestOut);
    const auto& depsOut = (itOut != dependencyGraph.end()) ? itOut->second : std::vector<const Dependency*>{};
    currentBag.addPackage(*bestOut, depsOut);

    return true;
}

/**
 * Evaluate swap: ensure swap improves benefit and is feasible capacity-wise (calls bag.canSwap)
 */
bool GRASP::evaluateSwap(const Bag &currentBag, const Package *packageIn, Package *packageOut, int bagSize, int currentBenefit, int &benefitIncrease) const
{
    benefitIncrease = packageOut->getBenefit() - packageIn->getBenefit();
    if (benefitIncrease <= 0) return false;
    // rely on bag's feasibility check for dependencies and capacity
    return currentBag.canSwap(*packageIn, *packageOut, bagSize);
}

/**
 * Perturbation: remove up to removeCount packages randomly (respecting dependency constraints would require more logic),
 * then attempt to refill with a limited construction (small alpha).
 */
Bag* GRASP::perturbSolution(const Bag& source, int bagSize,
                            const std::vector<Package*>& allPackages,
                            const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
                            int removeCount)
{
    // start from a copy
    Bag* pert = new Bag(source);

    // gather package pointers currently in bag
    std::vector<const Package*> inBagVec(pert->getPackages().begin(), pert->getPackages().end());
    if (inBagVec.empty()) return pert;

    // remove up to removeCount random packages (preferably random distinct)
    int toRemove = std::min(removeCount, (int)inBagVec.size());
    std::uniform_int_distribution<> dist(0, int(inBagVec.size() - 1));

    for (int i = 0; i < toRemove; ++i) {
        int idx = dist(m_generator);
        const Package* p = inBagVec[idx];

        auto it = dependencyGraph.find(p);
        const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};

        // attempt removal (Bag::removePackage should handle dependencies)
        pert->removePackage(*p, deps);

        // remove from vector (swap/pop)
        std::swap(inBagVec[idx], inBagVec.back());
        inBagVec.pop_back();
        if (inBagVec.empty()) break;
        dist = std::uniform_int_distribution<>(0, int(inBagVec.size() - 1));
    }

    // small reconstruction: limited greedy-random filling with small alpha
    double smallAlpha = 0.2;
    // build candidate list of packages not in pert
    std::vector<Package*> candidates;
    candidates.reserve(allPackages.size());
    for (Package* p : allPackages) {
        if (pert->getPackages().count(p) == 0) candidates.push_back(p);
    }

    // reuse construction logic (inline simplified): rank by ratio, build RCL, add until none fit
    while (!candidates.empty()) {
        std::vector<std::pair<Package*, double>> scored;
        for (Package* p : candidates) {
            if (p->getDependenciesSize() <= 0) continue;
            scored.emplace_back(p, static_cast<double>(p->getBenefit()) / p->getDependenciesSize());
        }
        if (scored.empty()) break;
        std::sort(scored.begin(), scored.end(), [](auto &a, auto &b){ return a.second > b.second; });
        double best = scored.front().second;
        double worst = scored.back().second;
        double threshold = best - smallAlpha * (best - worst);

        std::vector<Package*> rcl;
        for (auto &pr : scored) {
            if (pr.second >= threshold) rcl.push_back(pr.first);
            else break;
        }
        if (rcl.empty()) break;
        Package* pick = rcl[randomNumberInt(0, (int)rcl.size() - 1)];
        auto it = dependencyGraph.find(pick);
        const std::vector<const Dependency*> deps = (it != dependencyGraph.end()) ? it->second : std::vector<const Dependency*>{};
        if (pert->canAddPackage(*pick, bagSize, deps)) pert->addPackage(*pick, deps);

        // remove pick from candidates
        candidates.erase(std::remove(candidates.begin(), candidates.end(), pick), candidates.end());
    }

    return pert;
}

/**
 * Choose an alpha index according to m_alphaProbs (categorical distribution)
 */
double GRASP::pickAlpha()
{
    // create categorical distribution
    std::discrete_distribution<int> dist(m_alphaProbs.begin(), m_alphaProbs.end());
    int idx = dist(m_generator);
    m_alphaCounts[idx] += 1;
    return m_alphas[idx];
}

/**
 * Update alpha rewards using a simple incremental average: score += improvement
 * then update probabilities by normalizing scores (with small epsilon).
 */
void GRASP::updateAlphaRewards(double alpha, double improvement)
{
    // find index
    auto it = std::find(m_alphas.begin(), m_alphas.end(), alpha);
    if (it == m_alphas.end()) return;
    int idx = static_cast<int>(std::distance(m_alphas.begin(), it));
    m_alphaScores[idx] += std::max(0.0, improvement); // only reward positive improvement

    // recompute probabilities proportional to (score/count) or fallback to uniform if zero
    std::vector<double> values(m_alphas.size(), 0.0);
    double total = 0.0;
    for (size_t i = 0; i < m_alphas.size(); ++i) {
        if (m_alphaCounts[i] > 0) values[i] = m_alphaScores[i] / static_cast<double>(m_alphaCounts[i]);
        else values[i] = 0.0;
        // small floor to avoid zeros killing distribution
        values[i] = values[i] + 1e-6;
        total += values[i];
    }
    if (total <= 0.0) {
        // fallback uniform
        double u = 1.0 / static_cast<double>(m_alphaProbs.size());
        for (auto &p : m_alphaProbs) p = u;
    } else {
        for (size_t i = 0; i < m_alphaProbs.size(); ++i) m_alphaProbs[i] = values[i] / total;
    }
}

/**
 * Utility random integer in [min, max]
 */
int GRASP::randomNumberInt(int min, int max)
{
    if (min >= max) return min;
    std::uniform_int_distribution<> dist(min, max);
    return dist(m_generator);
}
