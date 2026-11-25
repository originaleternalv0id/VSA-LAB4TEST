#include "BenchmarkUtils.h"
#include "HashUtils.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& message = "Check failed.") {
    if (!condition) throw std::runtime_error(message);
}
template <typename Exception, typename Function>
void expectThrows(Function action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown.");
}
std::vector<std::string> collidingKeys(std::size_t count) {
    std::vector<std::string> result;
    for (std::size_t i = 0; i < 10000 && result.size() < count; ++i) {
        const auto key = "key-" + std::to_string(i);
        if (HashUtils::polynomialHash(key) % 7 == 0) result.push_back(key);
    }
    require(result.size() == count);
    return result;
}
std::string loadCsv(const std::vector<Benchmark::ArchitectureResult>& rows) {
    std::ostringstream output;
    Benchmark::writeLoadCsv(output, rows);
    return output.str();
}
std::vector<Benchmark::ArchitectureResult> manualRows() {
    return Benchmark::compareArchitectures({"A", "H", "O"}, {"V"}, 7, 3.0 / 7);
}
void testReproducibleDataset() {
    require(Benchmark::generateKeys(10000, 42) == Benchmark::generateKeys(10000, 42));
}
void testUniqueness() {
    const auto keys = Benchmark::generateKeys();
    require(std::set<std::string>(keys.begin(), keys.end()).size() == 10000);
}
void testDatasetSize() {
    require(Benchmark::generateKeys().size() == 10000);
    require(Benchmark::generateKeys(1).size() == 1 && Benchmark::generateKeys(0).empty());
}
void testSeedEffect() {
    require(Benchmark::generateKeys(100, 42) != Benchmark::generateKeys(100, 43));
}
void testKeyFormat() {
    for (const auto& key : Benchmark::generateKeys(100)) {
        require(key.size() == 13 && key.substr(0, 3) == "978");
        require(std::all_of(key.begin(), key.end(), [](char c) { return c >= '0' && c <= '9'; }));
    }
}
void testTargetCounts() {
    require(Benchmark::targetElementCount(10007, 0.25) == 2502);
    require(Benchmark::targetElementCount(10007, 0.50) == 5004);
    require(Benchmark::targetElementCount(10007, 0.75) == 7505);
    require(Benchmark::targetElementCount(10007, 0.90) == 9006);
}
void testRounding() {
    require(Benchmark::targetElementCount(2, 0.25) == 1);
    require(Benchmark::targetElementCount(2, 0.75) == 2);
    require(Benchmark::targetElementCount(7, 0) == 0 && Benchmark::targetElementCount(7, 1) == 7);
}
void testRawCollisions() {
    const auto row = Benchmark::measureDistribution({"AB","BA","C@"}, 7, 1, CompressionMode::MODULO);
    require(row.datasetSize == 3 && row.uniqueHashes == 1 && row.rawCollisions == 2);
    require(row.occupiedBuckets == 1 && row.indexCollisions == 2 && row.maxBucketOccupancy == 3);
}
void testIndexCollisions() {
    const auto row = Benchmark::measureDistribution({"A","H","O"}, 7, 37, CompressionMode::MODULO);
    require(row.uniqueHashes == 3 && row.rawCollisions == 0);
    require(row.occupiedBuckets == 1 && row.indexCollisions == 2); // Not three colliding pairs.
}
void testBaseThirtySeven() {
    const auto row = Benchmark::measureDistribution({"AB","BA","C@"}, 7, 37, CompressionMode::MODULO);
    require(row.uniqueHashes == 3 && row.rawCollisions == 0 && row.occupiedBuckets == 3);
    require(row.indexCollisions == 0 && row.maxBucketOccupancy == 1);
}
void testMadMetadataAndRawEquality() {
    const std::vector<std::string> keys{"AB","BA","C@"};
    const auto modulo = Benchmark::measureDistribution(keys, 7, 1, CompressionMode::MODULO);
    const auto mad = Benchmark::measureDistribution(keys, 7, 1, CompressionMode::MAD);
    require(mad.rawCollisions == modulo.rawCollisions && mad.indexCollisions == 2);
    require(mad.madA == HashUtils::defaultMadA && mad.madB == HashUtils::defaultMadB &&
            mad.madP == HashUtils::defaultMadPrime);
    require(modulo.madA == 0 && modulo.madB == 0 && modulo.madP == 0);
}
void testAverage() {
    require(Benchmark::average(10, 4) == 2.5 && Benchmark::average(0, 0) == 0);
    expectThrows<std::invalid_argument>([] { Benchmark::average(1, 0); });
}
void testMissingGeneration() {
    const auto missing = Benchmark::generateMissingKeys(3, {"missing-0", "abc"});
    require(missing == std::vector<std::string>({"missing-1","missing-2","missing-3"}));
    require(Benchmark::generateMissingKeys(0, {}).empty());
    const auto dataset = Benchmark::generateKeys();
    const auto queries = Benchmark::generateMissingKeys(1000, dataset);
    const std::set<std::string> known(dataset.begin(), dataset.end());
    require(std::set<std::string>(queries.begin(), queries.end()).size() == 1000);
    for (const auto& key : queries) require(known.count(key) == 0);
}
void testFixedCapacity() {
    const auto keys = Benchmark::generateKeys(100);
    for (double load : {0.25, 0.50, 0.75, 0.90}) {
        for (const auto& row : Benchmark::compareArchitectures(keys, {"missing"}, 101, load)) {
            require(row.capacity == 101 && row.deletedSlots == 0);
        }
    }
}
void testManualSuccessfulSearches() {
    const auto rows = manualRows();
    require(rows.size() == 3);
    for (const auto& row : rows) {
        require(row.attempted == 3 && row.successful == 3 && row.failed == 0 && row.targetAchieved);
        require(row.successSearch.queries == 3 && row.successSearch.probes == 6);
        require(row.successSearch.keyComparisons == 6 && Benchmark::average(row.successSearch.probes, 3) == 2);
    }
    require(rows[0].successSearch.accesses == 3 && rows[1].successSearch.accesses == 6);
    require(rows[0].maxLength == 3 && rows[1].maxLength == 3 && rows[2].maxLength == 2);
    require(rows[0].insertionCollisions == 2 && rows[1].insertionCollisions == 3 &&
            rows[2].insertionCollisions == 3);
}
void testManualMissingSearches() {
    const auto rows = manualRows();
    require(rows[0].missingSearch.probes == 3);
    require(rows[1].missingSearch.probes == 4 && rows[2].missingSearch.probes == 4);
    for (const auto& row : rows) {
        require(row.missingSearch.queries == 1 && row.missingSearch.keyComparisons == 3);
    }
}
void testQuadraticFailureAccounting() {
    auto keys = collidingKeys(7);
    const auto missing = keys.back();
    keys.pop_back();
    const auto rows = Benchmark::compareArchitectures(keys, {missing}, 7, 0.90);
    require(rows[0].successful == 6 && rows[1].successful == 6);
    const auto& quadratic = rows[2];
    require(quadratic.capacity == 7 && quadratic.attempted == 6 && quadratic.successful == 4);
    require(quadratic.failed == 2 && !quadratic.targetAchieved);
    require(quadratic.successSearch.queries == 4 && quadratic.missingSearch.probes == 4);
    require(std::abs(quadratic.actualLoad - 4.0 / 7) < 1e-12);
}
void testCommonSuccessfulSubset() {
    const auto rows = Benchmark::compareArchitectures(collidingKeys(6), {"missing"}, 7, 0.90);
    for (const auto& row : rows) {
        require(row.commonSearch.queries == 4 && row.commonSearch.probes == 10);
        require(row.commonSearch.keyComparisons == 10);
    }
    require(rows[0].successSearch.queries == 6 && rows[2].successSearch.queries == 4);
}
void testAccountingIdentities() {
    const auto keys = Benchmark::generateKeys(100);
    const auto missing = Benchmark::generateMissingKeys(30, keys);
    for (const auto& row : Benchmark::compareArchitectures(keys, missing, 101, 0.90)) {
        require(row.attempted == row.successful + row.failed);
        require(row.successSearch.queries == row.successful && row.missingSearch.queries == 30);
        require(row.commonSearch.queries <= row.successSearch.queries);
        require(row.targetAchieved == (row.successful == row.targetCount));
        require(std::abs(row.actualLoad - static_cast<double>(row.successful) / 101) < 1e-12);
    }
}
void testCsvStructure() {
    const auto csv = loadCsv(manualRows());
    std::istringstream input(csv);
    std::string line;
    require(static_cast<bool>(std::getline(input, line)));
    require(line.find("architecture,dataset_size,capacity,seed") == 0);
    require(line.find("common_successful_total_key_comparisons") != std::string::npos);
    require(std::count(line.begin(), line.end(), ',') == 39);
    int rows = 0;
    while (std::getline(input, line)) {
        require(std::count(line.begin(), line.end(), ',') == 39);
        ++rows;
    }
    require(rows == 3);
}
void testDistributionCsv() {
    const auto row = Benchmark::measureDistribution({"A","H","O"}, 7, 37, CompressionMode::MODULO);
    std::ostringstream polynomial, compression;
    Benchmark::writePolynomialCsv(polynomial, {row});
    Benchmark::writeCompressionCsv(compression, {row});
    require(polynomial.str() == compression.str());
    require(polynomial.str().find("3,42,37,7,\"MODULO\",0,0,0,3,0,1,2,3\n") != std::string::npos);
}
void testCsvEscaping() {
    auto rows = manualRows();
    rows[0].architecture = "A,\"B\"";
    require(loadCsv(rows).find("\"A,\"\"B\"\"\"") != std::string::npos);
}
void testIdenticalConfigurations() {
    const auto keys = Benchmark::generateKeys(50);
    const auto missing = Benchmark::generateMissingKeys(10, keys);
    require(loadCsv(Benchmark::compareArchitectures(keys, missing, 53, 0.9)) ==
            loadCsv(Benchmark::compareArchitectures(keys, missing, 53, 0.9)));
}
void testDeterministicMeasurements() {
    const auto first = Benchmark::generateKeys(100);
    const auto second = Benchmark::generateKeys(100);
    std::ostringstream a, b;
    for (const auto base : {1ULL, 37ULL}) {
        Benchmark::writePolynomialCsv(a, {Benchmark::measureDistribution(first, 101, base, CompressionMode::MODULO)});
        Benchmark::writePolynomialCsv(b, {Benchmark::measureDistribution(second, 101, base, CompressionMode::MODULO)});
    }
    require(a.str() == b.str());
}
void testInvalidInputs() {
    expectThrows<std::invalid_argument>([] { Benchmark::generateKeys(1000001); });
    expectThrows<std::invalid_argument>([] { Benchmark::targetElementCount(0, 0.5); });
    expectThrows<std::invalid_argument>([] { Benchmark::targetElementCount(7, -0.1); });
    expectThrows<std::invalid_argument>([] { Benchmark::targetElementCount(7, 1.1); });
    expectThrows<std::invalid_argument>([] { Benchmark::targetElementCount(7, std::numeric_limits<double>::quiet_NaN()); });
    expectThrows<std::invalid_argument>([] { Benchmark::targetElementCount(7, std::numeric_limits<double>::infinity()); });
    expectThrows<std::invalid_argument>([] { Benchmark::measureDistribution({"x","x"}, 7, 37, CompressionMode::MODULO); });
    expectThrows<std::invalid_argument>([] { Benchmark::measureDistribution({"x"}, 4, 37, CompressionMode::MODULO); });
    expectThrows<std::invalid_argument>([] { Benchmark::measureDistribution({"x"}, 7, 37, static_cast<CompressionMode>(99)); });
    expectThrows<std::invalid_argument>([] { Benchmark::compareArchitectures({"x"}, {}, 7, 0.9); });
    expectThrows<std::invalid_argument>([] { Benchmark::compareArchitectures({"x"}, {"x"}, 7, 0.1); });
    expectThrows<std::invalid_argument>([] { Benchmark::compareArchitectures({"x"}, {"m","m"}, 7, 0.1); });
}
void testEmptyMeasurements() {
    const auto distribution = Benchmark::measureDistribution({}, 7, 37, CompressionMode::MODULO);
    require(distribution.uniqueHashes == 0 && distribution.indexCollisions == 0 &&
            distribution.maxBucketOccupancy == 0);
    const auto rows = Benchmark::compareArchitectures({}, {"missing"}, 7, 0);
    for (const auto& row : rows) {
        require(row.successful == 0 && row.targetAchieved && row.successSearch.queries == 0);
        require(row.commonSearch.queries == 0 && row.missingSearch.keyComparisons == 0);
    }
    require(rows[0].missingSearch.probes == 0 && rows[1].missingSearch.probes == 1);
    require(!loadCsv(rows).empty());
}
void testDistributionMatchesChaining() {
    const auto keys = Benchmark::generateKeys(100);
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        const auto row = Benchmark::measureDistribution(keys, 101, 37, mode);
        SeparateChainingHash table(101, mode, 37, false);
        for (const auto& key : keys) require(table.insert({key,"","",true}));
        require(row.indexCollisions == table.collisionCount());
        require(row.maxBucketOccupancy == table.maxChainLength());
    }
}
void testOutputFailure() {
    std::ostream broken(nullptr);
    expectThrows<std::runtime_error>([&] { Benchmark::writeLoadCsv(broken, manualRows()); });
}
struct TestCase { const char* name; void (*run)(); };
}

int main() {
    const TestCase cases[] = {
        {"reproducible dataset",testReproducibleDataset}, {"dataset uniqueness",testUniqueness},
        {"dataset size",testDatasetSize}, {"seed effect",testSeedEffect}, {"ISBN-like format",testKeyFormat},
        {"required target counts",testTargetCounts}, {"half-up rounding",testRounding},
        {"raw hash collisions",testRawCollisions}, {"compressed-index collisions",testIndexCollisions},
        {"base 37 manual values",testBaseThirtySeven}, {"MAD metadata and raw equality",testMadMetadataAndRawEquality},
        {"arithmetic average",testAverage}, {"disjoint missing queries",testMissingGeneration},
        {"fixed capacities",testFixedCapacity}, {"manual hit probes and clusters",testManualSuccessfulSearches},
        {"manual miss probes",testManualMissingSearches}, {"quadratic failure accounting",testQuadraticFailureAccounting},
        {"common successful subset",testCommonSuccessfulSubset}, {"accounting identities",testAccountingIdentities},
        {"load CSV structure",testCsvStructure}, {"distribution CSV structure",testDistributionCsv},
        {"CSV escaping",testCsvEscaping}, {"identical configurations",testIdenticalConfigurations},
        {"deterministic measurements",testDeterministicMeasurements}, {"invalid inputs",testInvalidInputs},
        {"empty measurements",testEmptyMeasurements}, {"distribution matches real chaining",testDistributionMatchesChaining},
        {"output failure",testOutputFailure}
    };
    int failures = 0;
    for (const auto& test : cases) {
        try { test.run(); std::cout << "[PASS] " << test.name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n'; }
        catch (...) { ++failures; std::cerr << "[FAIL] " << test.name << ": unknown exception\n"; }
    }
    const auto count = sizeof(cases) / sizeof(cases[0]);
    std::cout << count - static_cast<std::size_t>(failures) << '/' << count << " tests passed.\n";
    return failures == 0 ? 0 : 1;
}
