#include "SeparateChainingHash.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace {
std::size_t normalizedCapacity(std::size_t requested, CompressionMode mode) {
    if (requested == 0) {
        throw std::invalid_argument("Initial capacity must be positive.");
    }
    if (mode != CompressionMode::MODULO && mode != CompressionMode::MAD) {
        throw std::invalid_argument("Unknown compression mode.");
    }
    if (mode == CompressionMode::MAD && requested >= HashUtils::defaultMadPrime) {
        throw std::invalid_argument("MAD capacity must be below the fixed prime.");
    }
    const auto result = HashUtils::isPrime(requested)
        ? requested : HashUtils::nextPrime(requested);
    if (mode == CompressionMode::MAD && result >= HashUtils::defaultMadPrime) {
        throw std::invalid_argument("Normalized MAD capacity must be below p.");
    }
    return result;
}
}

SeparateChainingHash::SeparateChainingHash(std::size_t initialCapacity,
    CompressionMode mode, std::uint64_t polynomialBase, bool autoResize)
    : buckets_(normalizedCapacity(initialCapacity, mode)),
      minimumCapacity_(buckets_.size()), mode_(mode),
      polynomialBase_(polynomialBase), autoResize_(autoResize),
      mad_(HashUtils::defaultMadA, HashUtils::defaultMadB, HashUtils::defaultMadPrime) {}

std::size_t SeparateChainingHash::bucketIndex(
    const std::string& isbn, std::size_t targetCapacity) const {
    const auto hash = HashUtils::polynomialHash(isbn, polynomialBase_);
    return mode_ == CompressionMode::MODULO
        ? HashUtils::moduloCompress(hash, targetCapacity)
        : mad_.compress(hash, targetCapacity);
}

bool SeparateChainingHash::insert(const Book& b) {
    const auto index = bucketIndex(b.isbn, capacity());
    auto& chain = buckets_[index];
    for (const auto& stored : chain) {
        if (stored.isbn == b.isbn) {
            return false;
        }
    }
    const bool collided = !chain.empty();
    if (collided && collisionCount_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error("Collision counter exhausted.");
    }
    chain.push_back(b);
    ++elementCount_;
    try {
        if (autoResize_ && elementCount_ >= capacity()) {
            if (capacity() > std::numeric_limits<std::size_t>::max() / 2) {
                throw std::overflow_error("Cannot double table capacity.");
            }
            rehash(HashUtils::nextPrime(2 * capacity()));
        }
    } catch (...) {
        // rehash commits only after all copies succeed, so rollback is safe.
        chain.pop_back();
        --elementCount_;
        throw;
    }
    if (collided) {
        ++collisionCount_;
    }
    return true;
}

bool SeparateChainingHash::remove(const std::string& isbn) {
    auto& chain = buckets_[bucketIndex(isbn, capacity())];
    for (auto it = chain.begin(); it != chain.end(); ++it) {
        if (it->isbn == isbn) {
            chain.erase(it);
            --elementCount_;
            // Exact integer equivalent of n/N < 0.25, avoiding 4*n overflow.
            if (autoResize_ && capacity() > minimumCapacity_ &&
                elementCount_ <= (capacity() - 1) / 4) {
                rehash(HashUtils::shrinkPrimeCapacity(capacity(), minimumCapacity_));
            }
            return true;
        }
    }
    return false;
}

const Book* SeparateChainingHash::findByISBN(const std::string& isbn) const {
    int ignored = 0;
    return findByISBN(isbn, ignored);
}

const Book* SeparateChainingHash::findByISBN(
    const std::string& isbn, int& probesOut) const {
    probesOut = 0;
    const auto& chain = buckets_[bucketIndex(isbn, capacity())];
    for (const auto& book : chain) {
        if (probesOut == std::numeric_limits<int>::max()) {
            throw std::overflow_error("Probe count exceeds int range.");
        }
        ++probesOut;
        if (book.isbn == isbn) {
            return &book;
        }
    }
    return nullptr;
}

void SeparateChainingHash::rehash(std::size_t newCapacity) {
    if (newCapacity == capacity()) {
        return;
    }
    if (mode_ == CompressionMode::MAD && newCapacity >= HashUtils::defaultMadPrime) {
        throw std::length_error("MAD capacity must remain smaller than p.");
    }
    std::vector<std::list<Book>> replacement(newCapacity);
    for (const auto& chain : buckets_) {
        for (const auto& book : chain) {
            replacement[bucketIndex(book.isbn, newCapacity)].push_back(book);
        }
    }
    // Commit only when every book was copied. Old pointers become invalid.
    buckets_.swap(replacement);
}

double SeparateChainingHash::loadFactor() const {
    return static_cast<double>(elementCount_) / static_cast<double>(capacity());
}

std::size_t SeparateChainingHash::size() const {
    return elementCount_;
}

std::size_t SeparateChainingHash::capacity() const {
    return buckets_.size();
}

std::size_t SeparateChainingHash::maxChainLength() const {
    std::size_t maximum = 0;
    for (const auto& chain : buckets_) {
        maximum = std::max(maximum, chain.size());
    }
    return maximum;
}

std::uint64_t SeparateChainingHash::collisionCount() const {
    return collisionCount_;
}
