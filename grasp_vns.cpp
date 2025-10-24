#include "grasp_vns.h"
#include "grasp_helper.h"
#include "vns_helper.h"

// ------------------- Constructor -------------------
GRASP_VNS::GRASP_VNS(double maxTime, unsigned int seed, int rclSize, double alpha)
    : m_maxTime(maxTime),
      m_alpha(alpha),
      m_alpha_random(alpha),
      m_rclSize(std::max(1, rclSize)),
      m_searchEngine(seed)
{
}

// ------------------- run -------------------
// This function (thread management) is identical to GRASP's
std::unique_ptr<Bag> GRASP_VNS::run(
    int bagSize,
    const std::vector<Package*>& allPackages,
    SearchEngine::MovementType moveType,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxLS_IterationsWithoutImprovement,
    int max_Iterations)
{
    // ... (Setup code is identical to original grasp.cpp) ...
    
    if (allPackages.empty()) {
        return std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::NONE, "0");
    }
    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(m_maxTime));
    std::unique_ptr<Bag> bestBagOverall = std::make_unique<Bag>(Algorithm::ALGORITHM_TYPE::NONE, "0");
    std::mutex bestBagMutex;
    unsigned int hw = std::thread::hardware_concurrency();
    unsigned int numThreads = hw == 0 ? 1u : hw;
    unsigned int cap = static_cast<unsigned int>(std::max<size_t>(1, dependencyGraph.size() / 100 + 1));
    numThreads = std::min(numThreads, cap);
    numThreads = std::min<unsigned int>(numThreads, static_cast<unsigned int>(std::max<size_t>(1, allPackages.size())));
    if (allPackages.size() < 200) numThreads = std::min<unsigned int>(numThreads, 2u);
    std::vector<std::thread> workers;
    workers.reserve(numThreads);
    m_totalIterations.store(0, std::memory_order_relaxed);
    m_improvements.store(0, std::memory_order_relaxed);

    for (unsigned int i = 0; i < numThreads; ++i) {
        WorkerContext ctx;
        ctx.bagSize = bagSize;
        ctx.allPackages = &allPackages;
        ctx.moveType = moveType; 
        ctx.dependencyGraph = &dependencyGraph;
        ctx.maxLS_IterationsWithoutImprovement = maxLS_IterationsWithoutImprovement;
        ctx.max_Iterations = max_Iterations;
        ctx.deadline = deadline;
        ctx.bestBagOverall = &bestBagOverall;
        ctx.bestBagMutex = &bestBagMutex;
        workers.emplace_back(&GRASP_VNS::graspWorker, this, std::move(ctx));
    }
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBagOverall->setAlgorithmTime(elapsed_seconds.count());
    bestBagOverall->setBagAlgorithm(Algorithm::ALGORITHM_TYPE::GRASP_VNS); 
    bestBagOverall->setLocalSearch(Algorithm::LOCAL_SEARCH::NONE);
    bestBagOverall->setMovementType(moveType);
    bestBagOverall->setMetaheuristicParameters(
        "Alpha: " + std::to_string(m_alpha_random) +
        " | VNS Improvements: " + std::to_string(m_improvements.load()) +
        " | RCL size: " + std::to_string(m_rclSize) +
        " | Total GRASP iterations: " + std::to_string(m_totalIterations.load())
    );
    return bestBagOverall;
}


// ------------------- Grasp Worker -------------------
void GRASP_VNS::graspWorker(WorkerContext ctx) {
    SearchEngine localEngine(m_searchEngine.getSeed());
    long long localIterations = 0;
    long long localImprovements = 0;

    thread_local std::vector<std::pair<Package*, double>> candidateScoresBuffer;
    thread_local std::vector<Package*> rclBuffer;

    std::unique_ptr<Bag> localBest;
    {
        std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
        localBest = std::make_unique<Bag>(*(*ctx.bestBagOverall));
    }

    const int timeCheckFreq = 16;
    while (localIterations < ctx.max_Iterations) {
        ++localIterations;

        // 1. GRASP Construction Phase
        std::unique_ptr<Bag> currentBag = GraspHelper::constructionPhaseFast(
            ctx.bagSize, *ctx.allPackages, *ctx.dependencyGraph, 
            localEngine,
            candidateScoresBuffer, 
            rclBuffer,
            m_rclSize,      // Pass member variable
            m_alpha,        // Pass member variable
            m_alpha_random  // Pass member variable (by ref)
        );
        
        double benefitBeforeVNS = currentBag->getBenefit();

        // 2. VNS Intensification Phase
        VnsHelper::vnsLoop(
            *currentBag, // Pass the bag to be improved
            ctx.bagSize,
            *ctx.allPackages,
            *ctx.dependencyGraph,
            localEngine, // Use the thread-local engine
            ctx.maxLS_IterationsWithoutImprovement,
            ctx.max_Iterations,
            ctx.deadline
        );

        // 3. Check for improvement
        if (currentBag->getBenefit() > localBest->getBenefit()) {
            if(currentBag->getBenefit() > benefitBeforeVNS) {
                ++localImprovements;
            }
            localBest = std::move(currentBag);
        }

        // ... (Syncing logic is identical to original grasp.cpp) ...
        if ((localIterations & 0x3) == 0) {
            std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
            if (localBest->getBenefit() > (*ctx.bestBagOverall)->getBenefit()) {
                *ctx.bestBagOverall = std::make_unique<Bag>(*localBest);
            }
        }
        if ((localIterations % timeCheckFreq) == 0) {
            if (std::chrono::steady_clock::now() >= ctx.deadline) break;
        }
    }
    
    // ... (Final sync and stats accumulation) ...
    {
        std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
        if (localBest->getBenefit() > (*ctx.bestBagOverall)->getBenefit()) {
            *ctx.bestBagOverall = std::make_unique<Bag>(*localBest);
        }
    }
    m_totalIterations.fetch_add(localIterations, std::memory_order_relaxed);
    m_improvements.fetch_add(localImprovements, std::memory_order_relaxed);
}