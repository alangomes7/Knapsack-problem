#include "random_provider.h"
#include <stdexcept> // For invalid_argument

namespace {
    // This anonymous namespace hides the generator and seed from other
    // translation units (files). They are global but private to this file.
    std::mt19937 g_generator;
    unsigned int g_seed = 0;
    bool g_initialized = false;
}

namespace RandomProvider {

void initialize(unsigned int seed) {
    g_seed = seed;
    g_generator.seed(g_seed);
    g_initialized = true;
}

unsigned int getSeed() {
    if (!g_initialized) {
        // Auto-initialize with a random device if getSeed() is called first
        initialize();
    }
    return g_seed;
}

// FIXED: Returns a reference (std::mt19937&) to the global generator
// and auto-initializes if needed.
std::mt19937& getGenerator()
{
    if (!g_initialized) {
        initialize(); // Auto-initialize if user forgot
    }
    return g_generator;
}

int getInt(int min, int max) {
    if (!g_initialized) {
        initialize(); // Auto-initialize if user forgot
    }
    if (min > max) {
        throw std::invalid_argument("RandomProvider::getInt: min cannot be greater than max");
    }
    std::uniform_int_distribution<> dist(min, max);
    return dist(g_generator);
}

double getDouble(double min, double max) {
    if (!g_initialized) {
        initialize(); // Auto-initialize if user forgot
    }
    if (min > max) {
        throw std::invalid_argument("RandomProvider::getDouble: min cannot be greater than max");
    }
    std::uniform_real_distribution<double> dist(min, max);
    return dist(g_generator);
}

} // namespace RandomProvider