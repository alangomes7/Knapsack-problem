#ifndef RANDOM_PROVIDER_H
#define RANDOM_PROVIDER_H

#include <random>

/**
 * @brief A utility namespace for providing random numbers.
 *
 * These functions use a generator provided by the caller.
 */
namespace RANDOM_PROVIDER {

    /**
     * @brief Gets a random integer within the specified range [min, max]
     * using the provided generator.
     * @param min The inclusive minimum value.
     * @param max The inclusive maximum value.
     * @param generator A reference to the random number generator to use.
     * @return A random integer.
     */
    int getInt(int min, int max, std::mt19937& generator);

    /**
     * @brief Gets a random double within the specified range [min, max]
     * using the provided generator.
     * @param min The inclusive minimum value.
     * @param max The inclusive maximum value.
     * @param generator A reference to the random number generator to use.
     * @return A random double.
     */
    double getDouble(double min, double max, std::mt19937& generator);

} // namespace RANDOM_PROVIDER

#endif // RANDOM_PROVIDER_H