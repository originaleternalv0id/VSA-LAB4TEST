#include "OpenAddressingHash.h"
#include "HashUtils.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
std::size_t checkedCapacity(std::size_t requested, ProbingMode mode) {
    if (requested == 0) throw std::invalid_argument("Capacity must be positive.");
    if (mode != ProbingMode::LINEAR && mode != ProbingMode::QUADRATIC) {
        throw std::invalid_argument("Unknown probing mode.");
    }
    // Probe counts use the assignment's int interface.
    const auto maximum = static_cast<std::size_t>(std::numeric_limits<int>::max());
    if (requested > maximum) throw std::length_error("Capacity exceeds int probe range.");
    const auto normalized = HashUtils::isPrime(requested)
        ? requested : HashUtils::nextPrime(requested);
    if (normalized > maximum) throw std::length_error("Prime capacity exceeds probe range.");
    return normalized;
}

// Both operands are already in [0, modulus). Avoid overflowing left+right.
std::size_t addModulo(std::size_t left, std::size_t right, std::size_t modulus) {
    return left >= modulus - right ? left - (modulus - right) : left + right;
}
}

OpenAddressingHash::OpenAddressingHash(std::size_t initialCapacity,
    ProbingMode mode, std::uint64_t polynomialBase, bool autoResize)
    : entries_(checkedCapacity(initialCapacity, mode)), mode_(mode),
      polynomialBase_(polynomialBase), autoResize_(autoResize) {}

OpenAddressingHash::ProbeResult OpenAddressingHash::locate(
    const std::string& ip, const std::vector<HashEntry>& slots) const {
    const auto count = slots.size();
    ProbeResult result{count};
    std::size_t firstDeleted = count;
    auto index = HashUtils::moduloCompress(HashUtils::polynomialHash(ip, polynomialBase_), count);
    std::size_t increment = 1;
    // For prime N, i=0..floor(N/2) covers all distinct quadratic residues.
    // N=2 also visits both slots. No repeated positions need to be inspected.
    const auto limit = mode_ == ProbingMode::LINEAR ? count : count / 2 + 1;
    for (std::size_t step = 0; step < limit; ++step) {
        ++result.probes;
        const auto& entry = slots[index];
        if (entry.status == EntryStatus::EMPTY) {
            result.index = firstDeleted != count ? firstDeleted : index;
            return result;
        }
        if (entry.status == EntryStatus::DELETED) {
            if (firstDeleted == count) firstDeleted = index;
        } else {
            ++result.keyComparisons;
            if (entry.ip == ip) {
                result.index = index;
                result.found = true;
                return result;
            }
        }
        // H_i = H_(i-1) + (2*i-1): successive increments 1, 3, 5, ...
        index = addModulo(index, increment, count);
        if (mode_ == ProbingMode::QUADRATIC) {
            increment = addModulo(increment, 2 % count, count);
        }
    }
    result.index = firstDeleted;
    return result;
}

bool OpenAddressingHash::insert(const std::string& ip) {
    auto position = locate(ip, entries_);
    if (position.found) return false;
    const auto encounters = static_cast<std::uint64_t>(position.keyComparisons);
    if (encounters > std::numeric_limits<std::uint64_t>::max() - collisionCount_) {
        throw std::overflow_error("Insertion collision counter exhausted.");
    }
    // Grow BEFORE inserting a new unique key that would make n/N > 0.5.
    // Duplicate attempts above never resize. Division avoids 2*(n+1) overflow.
    if (autoResize_ && (activeCount_ >= capacity() / 2 || position.index == capacity())) {
        if (capacity() > std::numeric_limits<std::size_t>::max() / 2) {
            throw std::overflow_error("Cannot double capacity.");
        }
        rehash(HashUtils::nextPrime(2 * capacity()));
        position = locate(ip, entries_);
    }
    if (position.index == capacity()) return false;
    auto& destination = entries_[position.index];
    destination.ip = ip; // If allocation throws, status/counters still describe a valid table.
    if (destination.status == EntryStatus::DELETED) --deletedCount_;
    destination.status = EntryStatus::ACTIVE;
    ++activeCount_;
    collisionCount_ += encounters;
    return true;
}

bool OpenAddressingHash::remove(const std::string& ip) {
    const auto position = locate(ip, entries_);
    if (!position.found) return false;
    auto& entry = entries_[position.index];
    entry.ip.clear();
    entry.status = EntryStatus::DELETED; // EMPTY would break other keys' probe paths.
    --activeCount_;
    ++deletedCount_;
    return true;
}

bool OpenAddressingHash::contains(const std::string& ip) const {
    int probes = 0;
    return contains(ip, probes);
}

bool OpenAddressingHash::contains(const std::string& ip, int& probesOut) const {
    int comparisons = 0;
    return contains(ip, probesOut, comparisons);
}

bool OpenAddressingHash::contains(
    const std::string& ip, int& probesOut, int& keyComparisonsOut) const {
    probesOut = 0;
    keyComparisonsOut = 0;
    const auto position = locate(ip, entries_);
    probesOut = position.probes;
    keyComparisonsOut = position.keyComparisons;
    return position.found;
}

void OpenAddressingHash::rehash(std::size_t newCapacity) {
    std::vector<HashEntry> replacement(checkedCapacity(newCapacity, mode_));
    for (const auto& entry : entries_) {
        if (entry.status != EntryStatus::ACTIVE) continue;
        const auto position = locate(entry.ip, replacement);
        if (position.found || position.index == replacement.size()) {
            throw std::logic_error("Rehash could not preserve every active key.");
        }
        replacement[position.index].ip = entry.ip;
        replacement[position.index].status = EntryStatus::ACTIVE;
    }
    // Commit after all transfers: allocation/copy errors leave old entries intact.
    entries_.swap(replacement);
    deletedCount_ = 0;
}

std::size_t OpenAddressingHash::size() const { return activeCount_; }
std::size_t OpenAddressingHash::capacity() const { return entries_.size(); }
std::size_t OpenAddressingHash::deletedCount() const { return deletedCount_; }
std::uint64_t OpenAddressingHash::collisionCount() const { return collisionCount_; }

double OpenAddressingHash::loadFactor() const {
    return static_cast<double>(activeCount_) / static_cast<double>(capacity());
}

double OpenAddressingHash::effectiveOccupancy() const {
    return static_cast<double>(activeCount_ + deletedCount_) / static_cast<double>(capacity());
}

std::size_t OpenAddressingHash::maxClusterLength() const {
    std::size_t empty = 0;
    while (empty < capacity() && entries_[empty].status != EntryStatus::EMPTY) ++empty;
    if (empty == capacity()) return capacity();
    std::size_t longest = 0;
    std::size_t current = 0;
    auto index = empty;
    // Starting at a known EMPTY slot joins suffix+prefix into one circular cluster.
    for (std::size_t visited = 0; visited < capacity(); ++visited) {
        index = addModulo(index, 1, capacity());
        if (entries_[index].status == EntryStatus::EMPTY) current = 0;
        else longest = std::max(longest, ++current);
    }
    return longest;
}
