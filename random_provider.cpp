#include "random_provider.h"
#include <stdexcept> // For invalid_argument

namespace RANDOM_PROVIDER {

int getInt(int min, int max, std::mt19937& generator) {
    if (min > max) {
        min = max;
    }
    std::uniform_int_distribution<> dist(min, max);
    return dist(generator);
}

double getDouble(double min, double max, std::mt19937& generator) {
    if (min > max) {
        min = max;
    }
    std::uniform_real_distribution<double> dist(min, max);
    return dist(generator);
}

} // namespace RANDOM_PROVIDER