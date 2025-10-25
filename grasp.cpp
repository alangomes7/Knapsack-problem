#include "grasp.h"
#include "grasp_helper.h"

static constexpr int DEFAULT_TIME_CHECK_FREQ = 10;             // check time every N iterations
static constexpr int DEFAULT_SYNC_FREQ = 10;                    // sync best bag every N iterations

GRASP::GRASP(double maxTime, unsigned int seed, int rclSize, double alpha)
    : m_maxTime(maxTime),
      m_alpha(alpha),
      m_alpha_random(alpha),
      m_rclSize(std::max(1, rclSize)),
      m_searchEngine(seed)
{
}

// ------------------- run -------------------
std::unique_ptr<Bag> GRASP::run(
    int bagSize,
    const std::vector<Package*>& allPackages,
    SEARCH_ENGINE::MovementType moveType,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxLS_IterationsWithoutImprovement,
    int max_Iterations)
{
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
        workers.emplace_back(&GRASP::graspWorker, this, std::move(ctx));
    }
    for (auto& w : workers) {
        if (w.joinable()) w.join();
    }
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    bestBagOverall->setAlgorithmTime(elapsed_seconds.count());
    bestBagOverall->setBagAlgorithm(ALGORITHM::ALGORITHM_TYPE::GRASP);
    bestBagOverall->setLocalSearch(ALGORITHM::LOCAL_SEARCH::NONE);
    bestBagOverall->setMovementType(moveType);
    std::cout << "GRASP completed: " << m_totalIterations.load() << " iterations, "
              << m_improvements.load() << " improvements" << std::endl;
    bestBagOverall->setMetaheuristicParameters(
        "Alpha: " + std::to_string(m_alpha_random) +
        " | Improvements: " + std::to_string(m_improvements.load()) +
        " | RCL size: " + std::to_string(m_rclSize) +
        " | Total iterations: " + std::to_string(m_totalIterations.load())
    );
    return bestBagOverall;
}

// ------------------- Grasp Worker -------------------
void GRASP::graspWorker(WorkerContext ctx) {
    unsigned int thread_seed;
    {
        std::lock_guard<std::mutex> lock(m_seeder_mutex);
        thread_seed = m_searchEngine.getRandomGenerator()();
    }
    SearchEngine localEngine(thread_seed);
    long long localIterations = 0;
    long long localImprovements = 0;

    thread_local std::vector<std::pair<Package*, double>> candidateScoresBuffer;
    thread_local std::vector<Package*> rclBuffer;

    // local copy of best bag
    std::unique_ptr<Bag> localBest;
    {
        std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
        localBest = std::make_unique<Bag>(*(*ctx.bestBagOverall));
    }

    auto workerStart = std::chrono::steady_clock::now();

    while (localIterations < ctx.max_Iterations) {
        ++localIterations;

        // 1. GRASP construction
        auto currentBag = GRASP_HELPER::constructionPhaseFast(
            ctx.bagSize, *ctx.allPackages, *ctx.dependencyGraph, localEngine,
            candidateScoresBuffer, rclBuffer,
            m_rclSize, m_alpha, m_alpha_random
        );

        // 2. Only run local search if solution is promising
        if (currentBag->getSize() < static_cast<int>(ctx.bagSize * 0.95) || currentBag->getBenefit() > localBest->getBenefit()) {
            localSearchPhase(localEngine, *currentBag, ctx.bagSize, *ctx.allPackages,
                             ctx.moveType, ALGORITHM::LOCAL_SEARCH::BEST_IMPROVEMENT,
                             *ctx.dependencyGraph, ctx.maxLS_IterationsWithoutImprovement,
                             ctx.max_Iterations, ctx.deadline);
        }

        // 3. Check improvement
        if (currentBag->getBenefit() > localBest->getBenefit()) {
            localBest = std::move(currentBag);
            ++localImprovements;
        }

        // 4. Batch-update global best
        if ((localIterations % DEFAULT_SYNC_FREQ) == 0) {
            std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
            if (localBest->getBenefit() > (*ctx.bestBagOverall)->getBenefit()) {
                *ctx.bestBagOverall = std::make_unique<Bag>(*localBest);
            }
        }

        // 5. Periodic time check
        if ((localIterations % DEFAULT_TIME_CHECK_FREQ) == 0) {
            if (std::chrono::steady_clock::now() >= ctx.deadline) break;
        }
    }

    // 6. Final sync
    {
        std::lock_guard<std::mutex> lk(*ctx.bestBagMutex);
        if (localBest->getBenefit() > (*ctx.bestBagOverall)->getBenefit()) {
            *ctx.bestBagOverall = std::make_unique<Bag>(*localBest);
        }
    }

    m_totalIterations.fetch_add(localIterations, std::memory_order_relaxed);
    m_improvements.fetch_add(localImprovements, std::memory_order_relaxed);
}

// ------------------- local Search Phase -------------------
// This function was unique to GRASP, so it stays.
void GRASP::localSearchPhase(
    SearchEngine& searchEngine,
    Bag& bag,
    int bagSize,
    const std::vector<Package*>& allPackages,
    SEARCH_ENGINE::MovementType moveType,
    ALGORITHM::LOCAL_SEARCH localSearchMethod,
    const std::unordered_map<const Package*, std::vector<const Dependency*>>& dependencyGraph,
    int maxLS_IterationsWithoutImprovement,
    int maxLS_Iterations,
    const std::chrono::steady_clock::time_point& deadline)
{
    if (bag.getBenefit() > 0 && bag.getSize() >= static_cast<int>(bagSize * 0.95)) return;

    searchEngine.localSearch(bag, bagSize, allPackages, moveType, localSearchMethod,
        dependencyGraph, maxLS_IterationsWithoutImprovement, maxLS_Iterations, deadline);
}