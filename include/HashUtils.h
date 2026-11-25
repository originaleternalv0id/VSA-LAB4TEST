#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace HashUtils {
inline constexpr std::uint64_t defaultMadPrime = 4294967291ULL;
inline constexpr std::uint64_t defaultMadA = 1664525;
inline constexpr std::uint64_t defaultMadB = 1013904223;

std::uint64_t polynomialHash(const std::string& key, std::uint64_t base = 37);
std::size_t moduloCompress(std::uint64_t hash, std::size_t capacity);

// Supported p range: prime 2 <= p <= UINT32_MAX.
// Validating once avoids repeating primality checks during table operations.
class MadCompressor {
public:
    MadCompressor(std::uint64_t a, std::uint64_t b, std::uint64_t p);
    std::size_t compress(std::uint64_t hash, std::size_t capacity) const;

private:
    std::uint64_t a_;
    std::uint64_t b_;
    std::uint64_t p_;
};

std::size_t madCompress(std::uint64_t hash, std::size_t capacity,
                        std::uint64_t a, std::uint64_t b, std::uint64_t p);
bool isPrime(std::size_t number);
std::size_t nextPrime(std::size_t number);

// Nearest prime to currentCapacity / 2.0; ties choose the smaller prime.
// minimumCapacity must be prime; result never goes below it.
std::size_t shrinkPrimeCapacity(std::size_t currentCapacity,
                                std::size_t minimumCapacity);
}
