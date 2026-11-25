#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class EntryStatus { ACTIVE, EMPTY, DELETED };
enum class ProbingMode { LINEAR, QUADRATIC };

struct HashEntry {
    std::string ip;
    EntryStatus status = EntryStatus::EMPTY;
};

class OpenAddressingHash {
public:
    explicit OpenAddressingHash(std::size_t initialCapacity = 11,
        ProbingMode mode = ProbingMode::LINEAR,
        std::uint64_t polynomialBase = 37, bool autoResize = true);

    bool insert(const std::string& ip);
    bool remove(const std::string& ip);
    bool contains(const std::string& ip) const;
    bool contains(const std::string& ip, int& probesOut) const;
    // Common comparison metric in addition to architecture-specific slot probes.
    bool contains(const std::string& ip, int& probesOut, int& keyComparisonsOut) const;

    std::size_t size() const;
    std::size_t capacity() const;
    double loadFactor() const;
    double effectiveOccupancy() const;
    std::size_t deletedCount() const;
    // ACTIVE and DELETED both occupy a cluster; circular boundary is included.
    std::size_t maxClusterLength() const;
    // ACTIVE encounters during the initial scan of successful user insertions.
    // Duplicates, failed inserts and internal rehash transfers do not count.
    std::uint64_t collisionCount() const;

private:
    struct ProbeResult {
        std::size_t index; // slots.size() means no reachable destination.
        bool found = false;
        int probes = 0;
        int keyComparisons = 0;
    };

    ProbeResult locate(const std::string& ip, const std::vector<HashEntry>& slots) const;
    void rehash(std::size_t newCapacity);

    std::vector<HashEntry> entries_;
    ProbingMode mode_;
    std::uint64_t polynomialBase_;
    bool autoResize_;
    std::size_t activeCount_ = 0;
    std::size_t deletedCount_ = 0;
    std::uint64_t collisionCount_ = 0;
};
