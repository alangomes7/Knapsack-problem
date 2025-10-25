#include "grasp_vns.h"
#include "grasp_helper.h"
#include "vns_helper.h"

// --- Add these tuning constants near top of file or inside GRASP_VNS as static members ---
static constexpr int DEFAULT_VNS_FREQUENCY = 2;                // run VNS every 2 GRASP iterations (set to 1 to always run)
static constexpr double DEFAULT_MIN_REMAINING_TIME_FOR_VNS = 0.5; // seconds; require at least this time to run VNS
static constexpr int DEFAULT_TIME_CHECK_FREQ = 10;             // check time every N iterations
static constexpr int DEFAULT_SYNC_FREQ = 10;                    // sync best bag every N iterations

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
    SEARCH_ENGINE::MovementType moveType,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxLS_IterationsWithoutImprovement,
    int max_Iterations)
{
    // ... (Setup code is identical to original grasp.cpp) ...
    
    if (allPackages.empty()) {
        return std::make_unique<Bag>(ALGORITHM::ALGORITHM_TYPE::NONE, "0");
    }
    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(m_maxTime));
    std::unique_ptr<Bag> bestBagOverall = std::make_unique<Bag>(ALGORITHM::ALGORITHM_TYPE::NONE, "0");
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
    bestBagOverall->setBagAlgorithm(ALGORITHM::ALGORITHM_TYPE::GRASP_VNS); 
    bestBagOverall->setLocalSearch(ALGORITHM::LOCAL_SEARCH::NONE);
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

    // local copy of the best bag (start from the global best)
    std::unique_ptr<Bag> localBest;
    {
        std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
        localBest = std::make_unique<Bag>(*(*ctx.bestBagOverall));
    }

    const int vnsFrequency = DEFAULT_VNS_FREQUENCY;
    const double minRemainingTimeForVNS = DEFAULT_MIN_REMAINING_TIME_FOR_VNS;
    const int timeCheckFreq = DEFAULT_TIME_CHECK_FREQ;
    const int syncFreq = DEFAULT_SYNC_FREQ;

    auto workerStart = std::chrono::steady_clock::now();

    while (localIterations < ctx.max_Iterations) {
        ++localIterations;

        // 1. GRASP Construction Phase (fast construction)
        std::unique_ptr<Bag> currentBag = GRASP_HELPER::constructionPhaseFast(
            ctx.bagSize, *ctx.allPackages, *ctx.dependencyGraph,
            localEngine,
            candidateScoresBuffer,
            rclBuffer,
            m_rclSize,
            m_alpha,
            m_alpha_random
        );

        double benefitBeforeVNS = currentBag->getBenefit();

        // Decide whether to run VNS now:
        bool runVnsThisIteration = (vnsFrequency <= 1) || ((localIterations % vnsFrequency) == 0);

        if (runVnsThisIteration) {
            // Compute remaining time safely (deadline may be shared across threads)
            auto now = std::chrono::steady_clock::now();
            const auto remaining = ctx.deadline - now;
            double remainingSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(remaining).count();

            // Only run VNS if we still have enough time left
            if (remainingSeconds >= minRemainingTimeForVNS) {
                // Call VNS intensification (existing heavy routine)
                VNS_HELPER::vnsLoop(
                    *currentBag,
                    ctx.bagSize,
                    *ctx.allPackages,
                    *ctx.dependencyGraph,
                    localEngine,
                    ctx.maxLS_IterationsWithoutImprovement / 2,
                    ctx.max_Iterations / 4,
                    ctx.deadline
                );
            } else {
                // Not enough time: skip VNS and keep the constructed solution
            }
        }

        // 3. Check for improvement
        if (currentBag->getBenefit() > localBest->getBenefit()) {
            if (currentBag->getBenefit() > benefitBeforeVNS) {
                ++localImprovements;
            }
            localBest = std::move(currentBag);
        }

        // Batch-update global best less often to reduce locking overhead
        if ((localIterations % syncFreq) == 0) {
            std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
            if (localBest->getBenefit() > (*ctx.bestBagOverall)->getBenefit()) {
                *ctx.bestBagOverall = std::make_unique<Bag>(*localBest);
            }
        }

        // Periodic time check to allow graceful exit before deadline
        if ((localIterations % timeCheckFreq) == 0) {
            if (std::chrono::steady_clock::now() >= ctx.deadline) break;
        }
    }

    // Final sync to global best
    {
        std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
        if (localBest->getBenefit() > (*ctx.bestBagOverall)->getBenefit()) {
            *ctx.bestBagOverall = std::make_unique<Bag>(*localBest);
        }
    }

    m_totalIterations.fetch_add(localIterations, std::memory_order_relaxed);
    m_improvements.fetch_add(localImprovements, std::memory_order_relaxed);
}