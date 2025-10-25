#include "random_provider.h"
#include <stdexcept> // For invalid_argument
#include <mutex>     // For thread-safety

namespace {
    // --- Global "Master Seeder" ---
    // This single generator will provide seeds for all threads.
    // It MUST be protected by a mutex.
    std::mt19937 g_seeder;
    std::mutex g_seeder_mutex;
    bool g_globally_initialized = false;
    unsigned int g_initial_seed = 0; // Store the seed for getSeed()

    // --- Thread-Local Storage ---
    // Each thread gets its OWN private generator.
    // This is fast and safe *after* initialization.
    thread_local std::mt19937 t_generator;
    thread_local unsigned int t_seed = 0;
    thread_local bool t_initialized = false;

    /**
     * @brief Initializes the global seeder if not already done.
     * This is the auto-initialize path.
     */
    void ensure_global_init() {
        if (g_globally_initialized) return;

        std::lock_guard<std::mutex> lock(g_seeder_mutex);
        if (g_globally_initialized) return; // Double-check

        // Auto-initialize with a non-deterministic seed
        g_initial_seed = std::random_device{}();
        g_seeder.seed(g_initial_seed);
        g_globally_initialized = true;
    }

    /**
     * @brief Initializes the calling thread's private generator.
     */
    void initialize_thread_local() {
        if (t_initialized) return;

        // Make sure the global seeder is ready
        ensure_global_init();

        // Lock, get one unique seed from the global seeder, then unlock
        std::lock_guard<std::mutex> lock(g_seeder_mutex);
        t_seed = g_seeder();
        
        // Seed this thread's private generator
        t_generator.seed(t_seed);
        t_initialized = true;
    }
} // namespace

namespace RANDOM_PROVIDER {

void initialize(unsigned int seed) {
    std::lock_guard<std::mutex> lock(g_seeder_mutex);
    g_initial_seed = seed;
    g_seeder.seed(g_initial_seed);
    g_globally_initialized = true;
}

unsigned int getSeed() {
    ensure_global_init();
    return g_initial_seed;
}

std::mt19937& getGenerator() {
    if (!t_initialized) {
        initialize_thread_local();
    }
    return t_generator;
}

int getInt(int min, int max) {
    if (!t_initialized) {
        initialize_thread_local();
    }
    if (min > max) {
        throw std::invalid_argument("RandomProvider::getInt: min cannot be greater than max");
    }
    std::uniform_int_distribution<> dist(min, max);
    return dist(t_generator);
}

double getDouble(double min, double max) {
    if (!t_initialized) {
        initialize_thread_local();
    }
    if (min > max) {
        throw std::invalid_argument("RandomProvider::getDouble: min cannot be greater than max");
    }
    std::uniform_real_distribution<double> dist(min, max);
    return dist(t_generator);
}

} // namespace RANDOM_PROVIDER