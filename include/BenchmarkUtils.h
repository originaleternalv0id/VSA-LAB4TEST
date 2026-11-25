#pragma once

#include "SeparateChainingHash.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace Benchmark {
inline constexpr std::uint32_t defaultSeed = 42;
inline constexpr std::size_t defaultCapacity = 10007;
inline constexpr std::size_t defaultDatasetSize = 10000;

struct DistributionResult {
    std::size_t datasetSize = 0;
    std::uint32_t seed = defaultSeed;
    std::uint64_t base = 37;
    std::size_t capacity = 0;
    std::string compression;
    std::uint64_t madA = 0, madB = 0, madP = 0;
    std::size_t uniqueHashes = 0, rawCollisions = 0;
    std::size_t occupiedBuckets = 0, indexCollisions = 0, maxBucketOccupancy = 0;
};

struct SearchTotals {
    std::size_t queries = 0;
    std::uint64_t probes = 0;
    std::uint64_t keyComparisons = 0;
    std::uint64_t accesses = 0; // Initial bucket accesses (chaining) or slot inspections (OA).
};

struct ArchitectureResult {
    std::string architecture;
    std::size_t datasetSize = 0, capacity = 0;
    std::uint32_t seed = defaultSeed;
    std::uint64_t base = 37;
    double targetLoad = 0, actualLoad = 0;
    std::size_t targetCount = 0, attempted = 0, successful = 0, failed = 0;
    bool targetAchieved = false;
    std::size_t maxLength = 0, deletedSlots = 0;
    std::uint64_t insertionCollisions = 0;
    std::string lengthMetric, collisionMetric, probeMetric;
    SearchTotals successSearch, missingSearch, commonSearch;
};

std::vector<std::string> generateKeys(std::size_t count = defaultDatasetSize,
                                      std::uint32_t seed = defaultSeed);
std::vector<std::string> generateMissingKeys(std::size_t count,
                                            const std::vector<std::string>& dataset);
std::size_t targetElementCount(std::size_t capacity, double targetLoad);
double average(std::uint64_t total, std::size_t count);

DistributionResult measureDistribution(const std::vector<std::string>& keys,
    std::size_t capacity, std::uint64_t base, CompressionMode mode,
    std::uint32_t seed = defaultSeed);

std::vector<ArchitectureResult> compareArchitectures(
    const std::vector<std::string>& keys, const std::vector<std::string>& missingKeys,
    std::size_t capacity, double targetLoad, std::uint32_t seed = defaultSeed,
    std::uint64_t base = 37);

void writePolynomialCsv(std::ostream& output, const std::vector<DistributionResult>& rows);
void writeCompressionCsv(std::ostream& output, const std::vector<DistributionResult>& rows);
void writeLoadCsv(std::ostream& output, const std::vector<ArchitectureResult>& rows);
}
