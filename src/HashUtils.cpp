#include "HashUtils.h"

#include <limits>
#include <stdexcept>

namespace HashUtils {
std::uint64_t polynomialHash(const std::string& key, std::uint64_t base) {
    std::uint64_t hash = 0;
    for (unsigned char character : key) {
        // Unsigned overflow is defined modulo 2^64.
        hash = hash * base + character;
    }
    return hash;
}

std::size_t moduloCompress(std::uint64_t hash, std::size_t capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("Capacity must be positive.");
    }
    return static_cast<std::size_t>(hash % capacity);
}

MadCompressor::MadCompressor(std::uint64_t a, std::uint64_t b, std::uint64_t p)
    : a_(a), b_(b), p_(p) {
    if (p < 2 || p > std::numeric_limits<std::uint32_t>::max() ||
        !isPrime(static_cast<std::size_t>(p))) {
        throw std::invalid_argument("MAD p must be a prime in the 32-bit range.");
    }
    if (a == 0 || a >= p || b >= p) {
        throw std::invalid_argument("MAD requires 1 <= a < p and 0 <= b < p.");
    }
}

std::size_t MadCompressor::compress(std::uint64_t hash, std::size_t capacity) const {
    if (capacity == 0 || capacity >= p_) {
        throw std::invalid_argument("MAD requires 0 < capacity < p.");
    }
    // With p <= 2^32-1, the largest intermediate is p*(p-1) < 2^64.
    const std::uint64_t reduced = hash % p_;
    return static_cast<std::size_t>(((a_ * reduced + b_) % p_) % capacity);
}

std::size_t madCompress(std::uint64_t hash, std::size_t capacity,
                        std::uint64_t a, std::uint64_t b, std::uint64_t p) {
    return MadCompressor(a, b, p).compress(hash, capacity);
}

bool isPrime(std::size_t number) {
    if (number < 2) {
        return false;
    }
    if (number % 2 == 0) {
        return number == 2;
    }
    // Division avoids the overflow that divisor*divisor could cause.
    for (std::size_t divisor = 3; divisor <= number / divisor; divisor += 2) {
        if (number % divisor == 0) {
            return false;
        }
    }
    return true;
}

std::size_t nextPrime(std::size_t number) {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    if (number == maximum) {
        throw std::overflow_error("No representable larger prime.");
    }
    std::size_t candidate = number + 1;
    if (candidate <= 2) {
        return 2;
    }
    if (candidate % 2 == 0) {
        ++candidate;
    }
    while (!isPrime(candidate)) {
        if (candidate > maximum - 2) {
            throw std::overflow_error("No representable larger prime.");
        }
        candidate += 2;
    }
    return candidate;
}

std::size_t shrinkPrimeCapacity(std::size_t currentCapacity,
                                std::size_t minimumCapacity) {
    if (!isPrime(minimumCapacity) || currentCapacity < minimumCapacity) {
        throw std::invalid_argument("Invalid minimum or current capacity.");
    }
    const std::size_t half = currentCapacity / 2;
    if (half <= minimumCapacity) {
        return minimumCapacity;
    }
    std::size_t lower = half;
    while (!isPrime(lower)) {
        --lower; // The prime minimum is a guaranteed lower bound.
    }
    const std::size_t upperStart = half + currentCapacity % 2;
    const std::size_t upper = isPrime(upperStart) ? upperStart : nextPrime(upperStart);
    // Distances to the exact half, multiplied by two; no floating-point rounding.
    const std::size_t lowerDistance = currentCapacity - lower - lower;
    const std::size_t upperDistance = upper - (currentCapacity - upper);
    return lowerDistance <= upperDistance ? lower : upper;
}
}
