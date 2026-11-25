#include "OpenAddressingHash.h"
#include "HashUtils.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <random>
#include <set>
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
std::vector<std::string> keysAt(std::size_t capacity, std::size_t bucket, std::size_t count) {
    std::vector<std::string> keys;
    for (std::size_t i = 0; i < 1000000 && keys.size() < count; ++i) {
        const auto key = "192.0.2." + std::to_string(i);
        if (HashUtils::moduloCompress(HashUtils::polynomialHash(key), capacity) == bucket) {
            keys.push_back(key);
        }
    }
    require(keys.size() == count, "Collision data generation failed.");
    return keys;
}
void fill(OpenAddressingHash& table, int count) {
    for (int i = 0; i < count; ++i) require(table.insert("key-" + std::to_string(i)));
}
void testEmpty() {
    OpenAddressingHash table;
    require(table.size() == 0 && table.capacity() == 11 && table.deletedCount() == 0);
    require(table.loadFactor() == 0 && table.effectiveOccupancy() == 0);
    require(table.maxClusterLength() == 0 && table.collisionCount() == 0);
}
void testInsertion() {
    OpenAddressingHash table;
    require(table.insert("192.0.2.1") && table.size() == 1);
}
void testDuplicate() {
    OpenAddressingHash table;
    require(table.insert("192.0.2.1") && !table.insert("192.0.2.1"));
    require(table.size() == 1 && table.collisionCount() == 0);
}
void testSuccessfulSearch() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(11, mode);
        require(table.insert("192.0.2.1") && table.contains("192.0.2.1"));
    }
}
void testMissingSearch() {
    OpenAddressingHash table;
    table.insert("192.0.2.1");
    require(!table.contains("192.0.2.2"));
}
void testRemoval() {
    OpenAddressingHash table;
    table.insert("192.0.2.1");
    require(table.remove("192.0.2.1") && !table.contains("192.0.2.1"));
    require(table.size() == 0 && table.deletedCount() == 1);
}
void testMissingRemoval() {
    OpenAddressingHash table;
    table.insert("192.0.2.1");
    require(!table.remove("missing") && table.size() == 1 && table.deletedCount() == 0);
}
void testLinearCollisions() {
    OpenAddressingHash table(11, ProbingMode::LINEAR, 37, false);
    const auto keys = keysAt(11, 3, 4);
    for (const auto& key : keys) require(table.insert(key));
    require(table.maxClusterLength() == 4 && table.collisionCount() == 6);
}
void testLinearWraparound() {
    OpenAddressingHash table(7, ProbingMode::LINEAR, 37, false);
    const auto keys = keysAt(7, 6, 3);
    for (const auto& key : keys) require(table.insert(key));
    int probes = 0;
    require(table.contains(keys[2], probes) && probes == 3);
    require(table.maxClusterLength() == 3);
}
void testQuadraticSequence() {
    const std::size_t positions[] = {0, 1, 4, 9, 5, 3};
    const auto query = keysAt(11, 0, 2)[1];
    for (std::size_t prefix = 0; prefix <= 6; ++prefix) {
        OpenAddressingHash table(11, ProbingMode::QUADRATIC, 37, false);
        for (std::size_t i = 0; i < prefix; ++i) {
            require(table.insert(keysAt(11, positions[i], 1)[0]));
        }
        int probes = -1;
        require(!table.contains(query, probes));
        require(probes == static_cast<int>(prefix < 6 ? prefix + 1 : 6),
                "Sequence must visit 0,1,4,9,5,3.");
    }
}
void testQuadraticCollisions() {
    OpenAddressingHash table(11, ProbingMode::QUADRATIC, 37, false);
    const auto keys = keysAt(11, 0, 6);
    for (const auto& key : keys) require(table.insert(key));
    require(table.maxClusterLength() == 3 && table.collisionCount() == 15);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        int probes = 0;
        require(table.contains(keys[i], probes) && probes == static_cast<int>(i + 1));
    }
}
void testQuadraticTermination() {
    OpenAddressingHash table(11, ProbingMode::QUADRATIC, 37, false);
    const auto keys = keysAt(11, 0, 7);
    for (std::size_t i = 0; i < 6; ++i) table.insert(keys[i]);
    int probes = 0;
    require(!table.contains(keys[6], probes) && probes == 6);
    require(!table.insert(keys[6]) && table.size() == 6 && table.capacity() == 11);
}
void testActiveStatus() {
    OpenAddressingHash table;
    table.insert("active");
    int probes = 0, comparisons = 0;
    require(table.contains("active", probes, comparisons) && probes == 1 && comparisons == 1);
}
void testEmptyStatus() {
    OpenAddressingHash table;
    int probes = 9, comparisons = 9;
    require(!table.contains("missing", probes, comparisons) && probes == 1 && comparisons == 0);
}
void testDeletedStatus() {
    OpenAddressingHash table;
    table.insert("gone");
    table.remove("gone");
    int probes = 0, comparisons = 0;
    require(!table.contains("gone", probes, comparisons) && probes == 2 && comparisons == 0);
}
void testSearchThroughDeleted() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(7, mode, 37, false);
        const auto keys = keysAt(7, 0, 3);
        for (const auto& key : keys) table.insert(key);
        table.remove(keys[0]);
        table.remove(keys[1]);
        int probes = 0, comparisons = 0;
        require(table.contains(keys[2], probes, comparisons) && probes == 3 && comparisons == 1);
    }
}
void testReuseDeleted() {
    OpenAddressingHash table(7, ProbingMode::LINEAR, 37, false);
    const auto keys = keysAt(7, 0, 3);
    table.insert(keys[0]);
    table.insert(keys[1]);
    table.remove(keys[0]);
    require(table.insert(keys[2]) && table.deletedCount() == 0 && table.size() == 2);
    int probes = 0;
    require(table.contains(keys[2], probes) && probes == 1);
}
void testDuplicateAfterTombstone() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(7, mode, 37, false);
        const auto keys = keysAt(7, 0, 3);
        for (const auto& key : keys) table.insert(key);
        table.remove(keys[0]);
        const auto collisions = table.collisionCount();
        require(!table.insert(keys[2]) && table.size() == 2 && table.deletedCount() == 1);
        require(table.collisionCount() == collisions);
    }
}
void testActiveCount() {
    OpenAddressingHash table;
    fill(table, 20);
    for (int i = 0; i < 10; ++i) require(table.remove("key-" + std::to_string(i)));
    require(table.size() == 10);
}
void testDeletedCount() {
    OpenAddressingHash table(11, ProbingMode::LINEAR, 37, false);
    const auto keys = keysAt(11, 0, 4);
    for (const auto& key : keys) table.insert(key);
    table.remove(keys[0]); table.remove(keys[1]);
    require(table.deletedCount() == 2);
    table.insert(keys[0]);
    require(table.deletedCount() == 1);
    require(!table.remove("missing") && table.deletedCount() == 1);
}
void testLoadFactor() {
    OpenAddressingHash table(11, ProbingMode::LINEAR, 37, false);
    fill(table, 3); table.remove("key-0");
    require(std::abs(table.loadFactor() - 2.0 / 11) < 1e-12);
    require(std::abs(table.effectiveOccupancy() - 3.0 / 11) < 1e-12);
}
void testExpansionThreshold() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(11, mode);
        fill(table, 5);
        require(table.capacity() == 11);
        table.insert("sixth");
        require(table.capacity() == 23 && table.size() == 6 && table.loadFactor() <= 0.5);
    }
    OpenAddressingHash tiny(2);
    tiny.insert("one");
    require(tiny.capacity() == 2 && tiny.loadFactor() == 0.5);
    tiny.insert("two");
    require(tiny.capacity() == 5);
}
void testExpansionPrime() {
    OpenAddressingHash table(3);
    table.insert("a"); table.insert("b");
    require(table.capacity() == 7);
    table.insert("c"); table.insert("d");
    require(table.capacity() == 17);
}
void testRehashPreservesKeys() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(3, mode);
        fill(table, 500);
        for (int i = 0; i < 500; ++i) require(table.contains("key-" + std::to_string(i)));
        require(table.size() == 500 && table.loadFactor() <= 0.5);
    }
}
void testRehashCleansTombstones() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(11, mode);
        std::vector<std::string> keys;
        for (std::size_t i = 0; i < 5; ++i) {
            keys.push_back(keysAt(11, i, 1)[0]);
            table.insert(keys.back());
        }
        table.remove(keys[0]);
        table.insert(keysAt(11, 8, 1)[0]);
        require(table.deletedCount() == 1);
        table.insert(keysAt(11, 9, 1)[0]);
        require(table.capacity() == 23 && table.deletedCount() == 0 && table.size() == 6);
        require(!table.contains(keys[0]));
        for (std::size_t i = 1; i < keys.size(); ++i) require(table.contains(keys[i]));
    }
}
void testFixedLinear() {
    OpenAddressingHash table(11, ProbingMode::LINEAR, 37, false);
    fill(table, 11);
    require(table.capacity() == 11 && table.loadFactor() == 1);
    require(!table.insert("overflow"));
}
void testFixedQuadratic() {
    OpenAddressingHash table(11, ProbingMode::QUADRATIC, 37, false);
    for (std::size_t i = 0; i < 11; ++i) require(table.insert(keysAt(11, i, 1)[0]));
    require(table.capacity() == 11 && table.loadFactor() == 1);
    require(!table.insert("overflow"));
}
void testFailedInsertPreservesState() {
    OpenAddressingHash table(7, ProbingMode::QUADRATIC, 37, false);
    const auto keys = keysAt(7, 0, 5);
    for (std::size_t i = 0; i < 4; ++i) table.insert(keys[i]);
    const auto collisions = table.collisionCount();
    require(!table.insert(keys[4]) && table.size() == 4 && table.capacity() == 7);
    require(table.deletedCount() == 0 && table.collisionCount() == collisions);
    for (std::size_t i = 0; i < 4; ++i) require(table.contains(keys[i]));
}
void testProbeCounting() {
    OpenAddressingHash table(7, ProbingMode::LINEAR, 37, false);
    const auto keys = keysAt(7, 0, 4);
    for (std::size_t i = 0; i < 3; ++i) table.insert(keys[i]);
    int probes = -1, comparisons = -1;
    require(table.contains(keys[2], probes, comparisons) && probes == 3 && comparisons == 3);
    table.remove(keys[0]);
    require(!table.contains(keys[3], probes, comparisons) && probes == 4 && comparisons == 2);
    require(!table.contains(keysAt(7, 6, 1)[0], probes, comparisons) && probes == 1 && comparisons == 0);
}
void testCollisionStatistics() {
    OpenAddressingHash table(7, ProbingMode::LINEAR, 37, false);
    const auto keys = keysAt(7, 0, 4);
    for (std::size_t i = 0; i < 3; ++i) table.insert(keys[i]);
    require(table.collisionCount() == 3);
    table.insert(keys[2]); table.contains(keys[0]); table.remove(keys[0]); table.remove("missing");
    require(table.collisionCount() == 3);
    table.insert(keys[3]);
    require(table.collisionCount() == 5); // Two ACTIVE comparisons, tombstone is not a collision.
}
void testMaximumCluster() {
    OpenAddressingHash table(11, ProbingMode::LINEAR, 37, false);
    for (const std::size_t slot : {1U, 2U, 3U, 7U}) table.insert(keysAt(11, slot, 1)[0]);
    require(table.maxClusterLength() == 3);
    table.remove(keysAt(11, 2, 1)[0]);
    require(table.maxClusterLength() == 3); // Tombstone remains occupied.
}
void testCircularCluster() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(7, mode, 37, false);
        for (const std::size_t slot : {5U, 6U, 0U, 1U}) table.insert(keysAt(7, slot, 1)[0]);
        require(table.maxClusterLength() == 4);
    }
}
void testThousands() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(11, mode);
        fill(table, 5000);
        for (int i = 0; i < 5000; ++i) require(table.contains("key-" + std::to_string(i)));
        for (int i = 0; i < 5000; ++i) require(table.remove("key-" + std::to_string(i)));
        require(table.size() == 0 && table.deletedCount() == 5000);
    }
}
void testRepeatedOperations() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(3, mode);
        for (int round = 0; round < 8; ++round) {
            fill(table, 120);
            for (int i = 0; i < 120; ++i) {
                require(!table.insert("key-" + std::to_string(i)));
                require(table.remove("key-" + std::to_string(i)));
            }
            require(table.size() == 0);
        }
    }
}
void testInvalidArguments() {
    expectThrows<std::invalid_argument>([] { OpenAddressingHash table(0); });
    expectThrows<std::invalid_argument>([] { OpenAddressingHash table(11, static_cast<ProbingMode>(99)); });
    expectThrows<std::length_error>([] { OpenAddressingHash table(std::numeric_limits<std::size_t>::max()); });
    OpenAddressingHash one(1), composite(10);
    require(one.capacity() == 2 && composite.capacity() == 11);
}
void testTombstoneWithoutEmpty() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(7, mode, 37, false);
        const auto keys = keysAt(7, 0, 8);
        const std::size_t reachable = mode == ProbingMode::LINEAR ? 7 : 4;
        for (std::size_t i = 0; i < reachable; ++i) require(table.insert(keys[i]));
        require(table.remove(keys[0]));
        require(!table.insert(keys[reachable - 1]) && table.deletedCount() == 1);
        require(table.insert(keys[7]) && table.deletedCount() == 0);
        require(table.contains(keys[7]) && table.size() == reachable);
    }
}
void testFullAndDeletedCluster() {
    OpenAddressingHash table(7, ProbingMode::LINEAR, 37, false);
    fill(table, 7);
    require(table.maxClusterLength() == 7);
    for (int i = 0; i < 7; ++i) table.remove("key-" + std::to_string(i));
    require(table.maxClusterLength() == 7 && table.loadFactor() == 0 && table.effectiveOccupancy() == 1);
    int probes = 0;
    require(!table.contains("missing", probes) && probes == 7);
}
void testGenericKeysAndBases() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(11, mode, 1);
        for (const auto& key : {std::string(), std::string("AB"), std::string("BA"),
                               std::string("a\0b", 3), std::string(10000, 'x')}) {
            require(table.insert(key) && table.contains(key) && !table.insert(key));
        }
        require(table.size() == 5);
    }
}
void testDuplicateDoesNotResize() {
    OpenAddressingHash table(11);
    fill(table, 5);
    require(!table.insert("key-4") && table.capacity() == 11);
}
void testRehashCollisionAccounting() {
    OpenAddressingHash table(3);
    const auto keys = keysAt(3, 0, 2);
    table.insert(keys[0]);
    table.insert(keys[1]);
    require(table.capacity() == 7 && table.collisionCount() == 1);
}
void testSeededReference() {
    for (auto mode : {ProbingMode::LINEAR, ProbingMode::QUADRATIC}) {
        OpenAddressingHash table(3, mode);
        std::set<std::string> reference;
        std::mt19937 random(42);
        for (int i = 0; i < 10000; ++i) {
            const auto key = std::to_string(random() % 600);
            switch (random() % 3) {
            case 0: require(table.insert(key) == reference.insert(key).second); break;
            case 1: require(table.remove(key) == (reference.erase(key) != 0)); break;
            default: require(table.contains(key) == (reference.count(key) != 0)); break;
            }
            require(table.size() == reference.size() && table.loadFactor() <= 0.5);
        }
        for (const auto& key : reference) require(table.contains(key));
    }
}
struct TestCase { const char* name; void (*run)(); };
}

int main() {
    const TestCase cases[] = {
        {"empty initialization", testEmpty}, {"insertion", testInsertion},
        {"duplicate rejection", testDuplicate}, {"successful search", testSuccessfulSearch},
        {"unsuccessful search", testMissingSearch}, {"removal", testRemoval},
        {"missing removal", testMissingRemoval}, {"linear collisions", testLinearCollisions},
        {"linear wraparound", testLinearWraparound}, {"incremental quadratic sequence", testQuadraticSequence},
        {"quadratic collisions", testQuadraticCollisions}, {"quadratic termination", testQuadraticTermination},
        {"ACTIVE handling", testActiveStatus}, {"EMPTY handling", testEmptyStatus},
        {"DELETED handling", testDeletedStatus}, {"search through deleted", testSearchThroughDeleted},
        {"reuse deleted", testReuseDeleted}, {"duplicate beyond tombstone", testDuplicateAfterTombstone},
        {"active count", testActiveCount}, {"deleted count", testDeletedCount},
        {"load and effective occupancy", testLoadFactor}, {"expansion threshold", testExpansionThreshold},
        {"expansion prime", testExpansionPrime}, {"rehash preservation", testRehashPreservesKeys},
        {"rehash tombstone cleanup", testRehashCleansTombstones}, {"fixed linear", testFixedLinear},
        {"fixed quadratic", testFixedQuadratic}, {"failure preserves state", testFailedInsertPreservesState},
        {"probe and comparison counts", testProbeCounting}, {"collision statistics", testCollisionStatistics},
        {"maximum cluster", testMaximumCluster}, {"circular cluster", testCircularCluster},
        {"5000 keys", testThousands}, {"repeated operations", testRepeatedOperations},
        {"invalid arguments", testInvalidArguments}, {"tombstone without EMPTY", testTombstoneWithoutEmpty},
        {"full and deleted clusters", testFullAndDeletedCluster}, {"generic strings and bases", testGenericKeysAndBases},
        {"duplicate does not resize", testDuplicateDoesNotResize}, {"rehash collision accounting", testRehashCollisionAccounting},
        {"seeded set reference", testSeededReference}
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
