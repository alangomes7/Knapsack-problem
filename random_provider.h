#ifndef RANDOM_PROVIDER_H
#define RANDOM_PROVIDER_H

#include <random>

/**
 * @brief A utility namespace for providing random numbers from a
 * single, globally shared generator.
 *
 * This utility auto-initializes with a random seed on first use.
 * For reproducible results, call `initialize(seed)` once at the
 * start of your program.
 */
namespace RandomProvider {

    /**
     * @brief Initializes (or re-initializes) the global random number generator.
     *
     * This is typically called once at the start of the program to
     * ensure reproducible results. If not called, the generator will
     * auto-initialize with a non-deterministic seed on its first use.
     *
     * @param seed The seed for the random number generator.
     * Defaults to a non-deterministic seed if not provided.
     */
    void initialize(unsigned int seed = std::random_device{}());

    /**
     * @brief Gets a random integer within the specified range [min, max]
     * using the shared global generator. (Auto-initializes if needed).
     * @param min The inclusive minimum value.
     * @param max The inclusive maximum value.
     * @return A random integer.
     */
    int getInt(int min, int max);

    /**
     * @brief Gets a random double within the specified range [min, max]
     * using the shared global generator. (Auto-initializes if needed).
     * @param min The inclusive minimum value.
     * @param max The inclusive maximum value.
     * @return A random double.
     */
    double getDouble(double min, double max);

    /**
     * @brief Returns the seed used to initialize the global generator.
     * (Auto-initializes with a random seed if not yet initialized).
     * @return The current seed.
     */
    unsigned int getSeed();

    /**
     * @brief Gets a reference to the shared global generator.
     *
     * This is useful for functions that require a generator reference,
     * e.g., `std::shuffle`. Using this function ensures that the
     * global generator's state is advanced.
     * (Auto-initializes if needed).
     * @return A reference to the global std::mt19937 generator.
     */
    std::mt19937& getGenerator();

} // namespace RandomProvider

#endif // RANDOM_PROVIDER_H