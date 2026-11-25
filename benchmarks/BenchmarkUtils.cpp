#include "BenchmarkUtils.h"
#include "HashUtils.h"
#include "OpenAddressingHash.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <random>
#include <set>
#include <stdexcept>

namespace {
void validateKeys(const std::vector<std::string>& keys) {
    if (keys.size() > 1000000) throw std::invalid_argument("Dataset exceeds one million keys.");
    const std::set<std::string> unique(keys.begin(), keys.end());
    if (unique.size() != keys.size()) throw std::invalid_argument("Dataset keys must be unique.");
}

void validateCapacity(std::size_t capacity) {
    if (capacity > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !HashUtils::isPrime(capacity)) {
        throw std::invalid_argument("Benchmark capacity must be prime and fit int probes.");
    }
}

void addSample(Benchmark::SearchTotals& totals, int probes, int comparisons, int accesses) {
    if (probes < 0 || comparisons < 0 || accesses < 0) {
        throw std::logic_error("Negative search instrumentation.");
    }
    ++totals.queries;
    totals.probes += static_cast<std::uint64_t>(probes);
    totals.keyComparisons += static_cast<std::uint64_t>(comparisons);
    totals.accesses += static_cast<std::uint64_t>(accesses);
}

void measureQuery(const SeparateChainingHash& table, const std::string& key,
                  bool expected, Benchmark::SearchTotals& totals) {
    int probes = 0;
    const bool found = table.findByISBN(key, probes) != nullptr;
    if (found != expected) throw std::logic_error("Chaining search disagrees with insertion results.");
    addSample(totals, probes, probes, 1);
}

void measureQuery(const OpenAddressingHash& table, const std::string& key,
                  bool expected, Benchmark::SearchTotals& totals) {
    int probes = 0, comparisons = 0;
    const bool found = table.contains(key, probes, comparisons);
    if (found != expected) throw std::logic_error("Open addressing search disagrees with insertion results.");
    addSample(totals, probes, comparisons, probes);
}

Benchmark::ArchitectureResult makeResult(const std::string& architecture,
    std::size_t datasetSize, std::size_t capacity, double targetLoad,
    std::uint32_t seed, std::uint64_t base, bool chaining) {
    Benchmark::ArchitectureResult row;
    row.architecture = architecture;
    row.datasetSize = datasetSize;
    row.capacity = capacity;
    row.seed = seed;
    row.base = base;
    row.targetLoad = targetLoad;
    row.targetCount = Benchmark::targetElementCount(capacity, targetLoad);
    row.lengthMetric = chaining ? "maximum_bucket_length" : "maximum_circular_nonempty_cluster";
    row.collisionMetric = chaining ? "occupied_bucket_insertions" : "active_encounters_initial_scan";
    row.probeMetric = chaining ? "stored_key_comparisons" : "inspected_slots";
    return row;
}

void finishResult(Benchmark::ArchitectureResult& row, std::size_t size,
    std::size_t actualCapacity, std::size_t maximum, std::uint64_t collisions,
    std::size_t deleted = 0) {
    if (actualCapacity != row.capacity || size > row.attempted || deleted != 0) {
        throw std::logic_error("Fixed-capacity benchmark invariant failed.");
    }
    row.successful = size;
    row.failed = row.attempted - size;
    row.targetAchieved = size == row.targetCount;
    row.actualLoad = static_cast<double>(size) / static_cast<double>(actualCapacity);
    row.maxLength = maximum;
    row.insertionCollisions = collisions;
    row.deletedSlots = deleted;
}

void csvText(std::ostream& output, const std::string& value) {
    output << '"';
    for (char character : value) {
        if (character == '"') output << '"';
        output << character;
    }
    output << '"';
}

void prepareCsv(std::ostream& output) {
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(10);
}

void writeDistribution(std::ostream& output, const Benchmark::DistributionResult& row) {
    output << row.datasetSize << ',' << row.seed << ',' << row.base << ',' << row.capacity << ',';
    csvText(output, row.compression);
    output << ',' << row.madA << ',' << row.madB << ',' << row.madP
           << ',' << row.uniqueHashes << ',' << row.rawCollisions
           << ',' << row.occupiedBuckets << ',' << row.indexCollisions
           << ',' << row.maxBucketOccupancy << '\n';
}

void writeSearch(std::ostream& output, const Benchmark::SearchTotals& totals) {
    output << ',' << totals.queries << ',' << totals.probes
           << ',' << Benchmark::average(totals.probes, totals.queries)
           << ',' << totals.keyComparisons
           << ',' << Benchmark::average(totals.keyComparisons, totals.queries)
           << ',' << totals.accesses
           << ',' << Benchmark::average(totals.accesses, totals.queries);
}

void checkOutput(const std::ostream& output) {
    if (!output) throw std::runtime_error("CSV write failed.");
}
}

namespace Benchmark {
std::vector<std::string> generateKeys(std::size_t count, std::uint32_t seed) {
    if (count > 1000000) throw std::invalid_argument("Dataset exceeds one million keys.");
    std::mt19937 generator(seed);
    std::set<std::string> seen;
    std::vector<std::string> keys;
    keys.reserve(count);
    // A bounded retry budget makes even dataset generation explicitly finite.
    const auto budget = count * 4 + 100;
    for (std::size_t attempt = 0; attempt < budget && keys.size() < count; ++attempt) {
        std::string key = "978";
        for (int digit = 0; digit < 10; ++digit) {
            key += static_cast<char>('0' + generator() % 10);
        }
        if (seen.insert(key).second) keys.push_back(key); // Preserve generation order.
    }
    if (keys.size() != count) throw std::runtime_error("Could not generate enough unique keys.");
    return keys;
}

std::vector<std::string> generateMissingKeys(std::size_t count,
                                            const std::vector<std::string>& dataset) {
    validateKeys(dataset);
    if (count > 1000000) throw std::invalid_argument("Too many missing queries.");
    const std::set<std::string> existing(dataset.begin(), dataset.end());
    std::vector<std::string> result;
    result.reserve(count);
    // At most dataset.size() candidates can be excluded by the finite dataset.
    for (std::size_t candidate = 0; result.size() < count; ++candidate) {
        auto key = "missing-" + std::to_string(candidate);
        if (existing.count(key) == 0) result.push_back(key);
    }
    return result;
}

std::size_t targetElementCount(std::size_t capacity, double targetLoad) {
    if (capacity == 0 || capacity > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        !std::isfinite(targetLoad) || targetLoad < 0 || targetLoad > 1) {
        throw std::invalid_argument("Invalid benchmark capacity or target load.");
    }
    // Nearest integer; exact .5 ties round upward.
    return static_cast<std::size_t>(
        std::floor(static_cast<long double>(capacity) * targetLoad + 0.5L));
}

double average(std::uint64_t total, std::size_t count) {
    if (count == 0) {
        if (total != 0) throw std::invalid_argument("Nonzero total with zero samples.");
        return 0;
    }
    return static_cast<double>(total) / static_cast<double>(count);
}

DistributionResult measureDistribution(const std::vector<std::string>& keys,
    std::size_t capacity, std::uint64_t base, CompressionMode mode, std::uint32_t seed) {
    validateKeys(keys);
    validateCapacity(capacity);
    if (mode != CompressionMode::MODULO && mode != CompressionMode::MAD) {
        throw std::invalid_argument("Unknown compression mode.");
    }
    DistributionResult row;
    row.datasetSize = keys.size(); row.seed = seed; row.base = base; row.capacity = capacity;
    row.compression = mode == CompressionMode::MODULO ? "MODULO" : "MAD";
    if (mode == CompressionMode::MAD) {
        row.madA = HashUtils::defaultMadA; row.madB = HashUtils::defaultMadB;
        row.madP = HashUtils::defaultMadPrime;
    }
    const HashUtils::MadCompressor mad(HashUtils::defaultMadA, HashUtils::defaultMadB,
                                      HashUtils::defaultMadPrime);
    std::set<std::uint64_t> hashes;
    std::vector<std::size_t> buckets(capacity, 0);
    for (const auto& key : keys) {
        const auto hash = HashUtils::polynomialHash(key, base);
        hashes.insert(hash);
        const auto index = mode == CompressionMode::MODULO
            ? HashUtils::moduloCompress(hash, capacity) : mad.compress(hash, capacity);
        if (buckets[index] == 0) ++row.occupiedBuckets;
        ++buckets[index];
        row.maxBucketOccupancy = std::max(row.maxBucketOccupancy, buckets[index]);
    }
    row.uniqueHashes = hashes.size();
    row.rawCollisions = keys.size() - hashes.size();
    row.indexCollisions = keys.size() - row.occupiedBuckets;
    return row;
}

std::vector<ArchitectureResult> compareArchitectures(
    const std::vector<std::string>& keys, const std::vector<std::string>& missingKeys,
    std::size_t capacity, double targetLoad, std::uint32_t seed, std::uint64_t base) {
    validateKeys(keys); validateKeys(missingKeys); validateCapacity(capacity);
    const auto target = targetElementCount(capacity, targetLoad);
    if (target > keys.size()) throw std::invalid_argument("Insufficient insertion candidates.");
    const std::set<std::string> known(keys.begin(), keys.end());
    for (const auto& key : missingKeys) {
        if (known.count(key) != 0) throw std::invalid_argument("Missing queries overlap the dataset.");
    }
    SeparateChainingHash chaining(capacity, CompressionMode::MODULO, base, false);
    OpenAddressingHash linear(capacity, ProbingMode::LINEAR, base, false);
    OpenAddressingHash quadratic(capacity, ProbingMode::QUADRATIC, base, false);
    auto chainRow = makeResult("Separate Chaining", keys.size(), capacity, targetLoad, seed, base, true);
    auto linearRow = makeResult("Linear Probing", keys.size(), capacity, targetLoad, seed, base, false);
    auto quadraticRow = makeResult("Quadratic Probing", keys.size(), capacity, targetLoad, seed, base, false);
    std::vector<bool> chainAccepted(target), linearAccepted(target), quadraticAccepted(target);
    for (std::size_t i = 0; i < target; ++i) {
        chainAccepted[i] = chaining.insert(Book{keys[i], "Synthetic title", "Synthetic author", true});
        linearAccepted[i] = linear.insert(keys[i]);
        quadraticAccepted[i] = quadratic.insert(keys[i]);
        ++chainRow.attempted; ++linearRow.attempted; ++quadraticRow.attempted;
    }
    finishResult(chainRow, chaining.size(), chaining.capacity(),
                 chaining.maxChainLength(), chaining.collisionCount());
    finishResult(linearRow, linear.size(), linear.capacity(),
                 linear.maxClusterLength(), linear.collisionCount(), linear.deletedCount());
    finishResult(quadraticRow, quadratic.size(), quadratic.capacity(),
                 quadratic.maxClusterLength(), quadratic.collisionCount(), quadratic.deletedCount());
    for (std::size_t i = 0; i < target; ++i) {
        if (chainAccepted[i]) measureQuery(chaining, keys[i], true, chainRow.successSearch);
        if (linearAccepted[i]) measureQuery(linear, keys[i], true, linearRow.successSearch);
        if (quadraticAccepted[i]) measureQuery(quadratic, keys[i], true, quadraticRow.successSearch);
        if (chainAccepted[i] && linearAccepted[i] && quadraticAccepted[i]) {
            measureQuery(chaining, keys[i], true, chainRow.commonSearch);
            measureQuery(linear, keys[i], true, linearRow.commonSearch);
            measureQuery(quadratic, keys[i], true, quadraticRow.commonSearch);
        }
    }
    for (const auto& key : missingKeys) {
        measureQuery(chaining, key, false, chainRow.missingSearch);
        measureQuery(linear, key, false, linearRow.missingSearch);
        measureQuery(quadratic, key, false, quadraticRow.missingSearch);
    }
    return {chainRow, linearRow, quadraticRow};
}

void writePolynomialCsv(std::ostream& output, const std::vector<DistributionResult>& rows) {
    prepareCsv(output);
    output << "dataset_size,seed,polynomial_base,capacity,compression,mad_a,mad_b,mad_p,"
              "unique_raw_hashes,raw_hash_collisions,occupied_buckets,compressed_index_collisions,"
              "maximum_bucket_occupancy\n";
    for (const auto& row : rows) writeDistribution(output, row);
    checkOutput(output);
}

void writeCompressionCsv(std::ostream& output, const std::vector<DistributionResult>& rows) {
    // Same explicit schema makes experiments A/B directly comparable.
    writePolynomialCsv(output, rows);
}

void writeLoadCsv(std::ostream& output, const std::vector<ArchitectureResult>& rows) {
    prepareCsv(output);
    output << "architecture,dataset_size,capacity,seed,polynomial_base,compression,target_load_factor,"
              "actual_load_factor,target_element_count,attempted_insertions,successful_insertions,"
              "failed_insertions,target_achieved,maximum_chain_or_cluster,length_metric,deleted_slots,"
              "insertion_collisions,collision_metric,probe_metric";
    for (const auto* prefix : {"successful", "unsuccessful", "common_successful"}) {
        output << ',' << prefix << "_queries," << prefix << "_total_probes,"
               << prefix << "_average_probes," << prefix << "_total_key_comparisons,"
               << prefix << "_average_key_comparisons," << prefix << "_total_array_or_bucket_accesses,"
               << prefix << "_average_array_or_bucket_accesses";
    }
    output << '\n';
    for (const auto& row : rows) {
        csvText(output, row.architecture);
        output << ',' << row.datasetSize << ',' << row.capacity << ',' << row.seed << ',' << row.base
               << ",MODULO," << row.targetLoad << ',' << row.actualLoad << ',' << row.targetCount
               << ',' << row.attempted << ',' << row.successful << ',' << row.failed
               << ',' << (row.targetAchieved ? "true" : "false") << ',' << row.maxLength << ',';
        csvText(output, row.lengthMetric);
        output << ',' << row.deletedSlots << ',' << row.insertionCollisions << ',';
        csvText(output, row.collisionMetric);
        output << ','; csvText(output, row.probeMetric);
        writeSearch(output, row.successSearch);
        writeSearch(output, row.missingSearch);
        writeSearch(output, row.commonSearch);
        output << '\n';
    }
    checkOutput(output);
}
}
