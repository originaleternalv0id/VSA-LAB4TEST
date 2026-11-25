#pragma once

#include "Book.h"
#include "HashUtils.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <string>
#include <vector>

enum class CompressionMode {
    MODULO,
    MAD
};

class SeparateChainingHash {
public:
    explicit SeparateChainingHash(std::size_t initialCapacity = 11,
        CompressionMode mode = CompressionMode::MODULO,
        std::uint64_t polynomialBase = 37, bool autoResize = true);

    bool insert(const Book& b);
    bool remove(const std::string& isbn);

    // Pointers are invalidated by removal of that book, rehash, assignment,
    // or destruction of the table. Reacquire after any mutation.
    const Book* findByISBN(const std::string& isbn) const;
    // A probe is one comparison against a stored ISBN; empty chains cost zero.
    const Book* findByISBN(const std::string& isbn, int& probesOut) const;

    double loadFactor() const;
    std::size_t size() const;
    std::size_t capacity() const;
    std::size_t maxChainLength() const;
    std::uint64_t collisionCount() const;

private:
    std::size_t bucketIndex(const std::string& isbn, std::size_t targetCapacity) const;
    void rehash(std::size_t newCapacity);

    std::vector<std::list<Book>> buckets_;
    std::size_t minimumCapacity_;
    CompressionMode mode_;
    std::uint64_t polynomialBase_;
    bool autoResize_;
    std::size_t elementCount_ = 0;
    std::uint64_t collisionCount_ = 0;
    HashUtils::MadCompressor mad_;
};
