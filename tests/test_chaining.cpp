#include "Book.h"
#include "HashUtils.h"
#include "SeparateChainingHash.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Exception, typename Function>
void expectThrows(Function action) {
    try {
        action();
    } catch (const Exception&) {
        return;
    }
    throw std::runtime_error("Expected exception was not thrown.");
}

Book makeBook(const std::string& isbn, bool available = true) {
    return {isbn, "Title " + isbn, "Author " + isbn, available};
}

void checkBook(const Book* actual, const Book& expected) {
    require(actual != nullptr, "Book is missing.");
    require(actual->isbn == expected.isbn && actual->title == expected.title &&
            actual->author == expected.author &&
            actual->isAvailable == expected.isAvailable, "Book fields changed.");
}

void fill(SeparateChainingHash& table, int count) {
    for (int i = 0; i < count; ++i) {
        require(table.insert(makeBook(std::to_string(i), i % 2 == 0)), "Insert failed.");
    }
}

void testEmpty() {
    SeparateChainingHash table;
    require(table.size() == 0 && table.capacity() == 11, "Empty size/capacity.");
    require(table.loadFactor() == 0 && table.maxChainLength() == 0 &&
            table.collisionCount() == 0, "Empty statistics.");
}

void testInsertion() {
    SeparateChainingHash table;
    require(table.insert(makeBook("001-ABC")), "Insert rejected.");
    require(table.size() == 1, "Size not incremented.");
}

void testDuplicate() {
    SeparateChainingHash table;
    const Book original = makeBook("001");
    require(table.insert(original), "Initial insert.");
    require(!table.insert({"001", "Changed", "Other", false}), "Duplicate accepted.");
    checkBook(table.findByISBN("001"), original);
    require(table.size() == 1 && table.collisionCount() == 0, "Duplicate changed state.");
}

void testLookup() {
    SeparateChainingHash table;
    const Book book{"000123-X", "Data Structures", "Student", false};
    table.insert(book);
    checkBook(table.findByISBN(book.isbn), book);
}

void testMissingLookup() {
    SeparateChainingHash table;
    table.insert(makeBook("known"));
    require(table.findByISBN("absent") == nullptr, "Unexpected lookup.");
}

void testRemoval() {
    SeparateChainingHash table;
    table.insert(makeBook("a"));
    require(table.remove("a") && table.size() == 0, "Removal failed.");
    require(table.findByISBN("a") == nullptr, "Removed book remains.");
}

void testMissingRemoval() {
    SeparateChainingHash table;
    table.insert(makeBook("a"));
    require(!table.remove("b"), "Missing removal succeeded.");
    require(table.size() == 1 && table.capacity() == 11 &&
            table.collisionCount() == 0, "Missing removal changed state.");
}

void testLoadFactor() {
    SeparateChainingHash table(11, CompressionMode::MODULO, 37, false);
    fill(table, 3);
    require(std::abs(table.loadFactor() - 3.0 / 11.0) < 1e-12, "Integer division.");
}

void testModulo() {
    require(HashUtils::moduloCompress(131, 11) == 10, "Modulo result.");
    require(HashUtils::moduloCompress(0, 11) == 0, "Zero hash.");
    require(HashUtils::moduloCompress(999, 1) == 0, "Capacity one.");
}

void testMad() {
    require(HashUtils::madCompress(100, 11, 3, 7, 101) == 4, "MAD result.");
    require(HashUtils::madCompress(0, 11, 3, 7, 101) == 7, "MAD zero hash.");
    HashUtils::MadCompressor cached(3, 7, 101);
    require(cached.compress(100, 11) == 4, "Cached MAD differs.");
}

void testBaseOne() {
    require(HashUtils::polynomialHash("AB", 1) == 131, "Base one sum.");
    require(HashUtils::polynomialHash("AB", 1) ==
            HashUtils::polynomialHash("BA", 1), "Anagrams should collide.");
}

void testBaseThirtySeven() {
    require(HashUtils::polynomialHash("AB") == 2471, "Horner result.");
    require(HashUtils::polynomialHash("BA") == 2507, "Order ignored.");
    require(HashUtils::polynomialHash("") == 0, "Empty hash.");
}

void testPrimes() {
    for (const std::size_t value : {2U, 3U, 11U, 97U, 10007U}) {
        require(HashUtils::isPrime(value), "Prime rejected.");
    }
    for (const std::size_t value : {0U, 1U, 4U, 9U, 25U, 49U, 10000U}) {
        require(!HashUtils::isPrime(value), "Composite accepted.");
    }
}

void testNextPrime() {
    require(HashUtils::nextPrime(0) == 2 && HashUtils::nextPrime(1) == 2 &&
            HashUtils::nextPrime(2) == 3 && HashUtils::nextPrime(11) == 13 &&
            HashUtils::nextPrime(22) == 23, "nextPrime must be strictly greater.");
}

void testForcedCollisions() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(5, mode, 1, false);
        for (const auto* key : {"AB", "BA", "C@"}) table.insert(makeBook(key));
        require(table.size() == 3 && table.maxChainLength() == 3, "Missing chain.");
        for (const auto* key : {"AB", "BA", "C@"}) checkBook(table.findByISBN(key), makeBook(key));
    }
}

void testCollisionCounter() {
    SeparateChainingHash table(5, CompressionMode::MODULO, 1, false);
    table.insert(makeBook("AB"));
    table.insert(makeBook("BA"));
    table.insert(makeBook("C@"));
    require(table.collisionCount() == 2, "Occupied-bucket convention.");
    table.insert(makeBook("AB"));
    table.findByISBN("D?");
    table.remove("D?");
    table.remove("BA");
    require(table.collisionCount() == 2, "Non-insert operation counted.");
    table.insert(makeBook("BA"));
    require(table.collisionCount() == 3, "Reinsert must count.");
}

void testMaximumChain() {
    SeparateChainingHash table(5, CompressionMode::MODULO, 1, false);
    table.insert(makeBook("AB"));
    table.insert(makeBook("BA"));
    table.insert(makeBook("A"));
    require(table.maxChainLength() == 2, "Max chain not two.");
    table.remove("AB");
    require(table.maxChainLength() == 1, "Max chain not updated.");
    table.remove("BA");
    table.remove("A");
    require(table.maxChainLength() == 0, "Empty maximum.");
}

void testExpansionThreshold() {
    SeparateChainingHash table;
    fill(table, 10);
    require(table.capacity() == 11, "Expanded too early.");
    table.insert(makeBook("10"));
    require(table.capacity() == 23 && table.size() == 11, "Threshold expansion.");
}

void testExpansionPrime() {
    SeparateChainingHash table(3);
    fill(table, 3);
    require(table.capacity() == 7, "3 must grow to 7.");
    for (int i = 3; i < 7; ++i) table.insert(makeBook(std::to_string(i)));
    require(table.capacity() == 17, "7 must grow to 17.");
}

void testExpansionPreservesBooks() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(3, mode);
        fill(table, 100);
        for (int i = 0; i < 100; ++i) {
            checkBook(table.findByISBN(std::to_string(i)), makeBook(std::to_string(i), i % 2 == 0));
        }
        require(table.size() == 100, "Rehash changed size.");
    }
}

void testShrinkThreshold() {
    SeparateChainingHash table;
    fill(table, 11);
    for (int i = 0; i < 5; ++i) table.remove(std::to_string(i));
    require(table.capacity() == 23 && table.size() == 6, "Shrank too early.");
    table.remove("5");
    require(table.capacity() == 11 && table.size() == 5, "Did not shrink.");
}

void testShrinkPreservesBooks() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(11, mode);
        fill(table, 11);
        for (int i = 0; i < 6; ++i) table.remove(std::to_string(i));
        require(table.capacity() == 11, "No shrink.");
        for (int i = 6; i < 11; ++i) {
            checkBook(table.findByISBN(std::to_string(i)), makeBook(std::to_string(i), i % 2 == 0));
        }
    }
}

void testMinimumCapacity() {
    SeparateChainingHash table(10);
    require(table.capacity() == 11, "Initial composite not normalized.");
    fill(table, 100);
    for (int i = 0; i < 100; ++i) table.remove(std::to_string(i));
    require(table.size() == 0 && table.capacity() == 11, "Minimum violated.");
    SeparateChainingHash tiny(1);
    require(tiny.capacity() == 2, "One must normalize to two.");
}

void testFixedCapacity() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(3, mode, 37, false);
        fill(table, 30);
        require(table.capacity() == 3 && table.loadFactor() == 10.0, "Fixed growth.");
        for (int i = 0; i < 30; ++i) {
            checkBook(table.findByISBN(std::to_string(i)), makeBook(std::to_string(i), i % 2 == 0));
            require(table.remove(std::to_string(i)), "Fixed removal.");
        }
        require(table.capacity() == 3, "Fixed shrink.");
        SeparateChainingHash benchmark(10007, mode, 37, false);
        require(benchmark.capacity() == 10007, "Benchmark capacity changed.");
    }
}

void testBothModes() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(5, mode);
        const Book book = makeBook("978-0000000001", false);
        require(table.insert(book) && !table.insert(book), "Mode insertion.");
        checkBook(table.findByISBN(book.isbn), book);
        require(table.remove(book.isbn) && !table.remove(book.isbn), "Mode removal.");
    }
}

void testProbes() {
    SeparateChainingHash table(5, CompressionMode::MODULO, 1, false);
    int probes = -10;
    require(!table.findByISBN("A", probes) && probes == 0, "Empty probes.");
    for (const auto* key : {"AB", "BA", "C@"}) table.insert(makeBook(key));
    require(table.findByISBN("AB", probes) && probes == 1, "First probes.");
    require(table.findByISBN("BA", probes) && probes == 2, "Second probes.");
    require(table.findByISBN("C@", probes) && probes == 3, "Third probes.");
    require(!table.findByISBN("D?", probes) && probes == 3, "Missing chain probes.");
    require(!table.findByISBN("A", probes) && probes == 0, "Probe reset.");
}

// Independent addition-and-doubling oracle; p is at most 32 bits here.
std::uint64_t referenceMad(std::uint64_t hash, std::uint64_t a,
                           std::uint64_t b, std::uint64_t p, std::size_t capacity) {
    std::uint64_t product = 0;
    std::uint64_t term = hash % p;
    while (a != 0) {
        if (a % 2 != 0) product = (product + term) % p;
        term = (term + term) % p;
        a /= 2;
    }
    return ((product + b) % p) % capacity;
}

void testLargeMad() {
    const auto p = HashUtils::defaultMadPrime;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    require(HashUtils::madCompress(maximum, 97, p - 1, p - 1, p) ==
            (p - 25) % 97, "Maximum hash arithmetic.");
    std::mt19937_64 random(20261125);
    for (int i = 0; i < 1000; ++i) {
        const auto hash = random();
        const auto a = 1 + random() % (p - 1);
        const auto b = random() % p;
        require(HashUtils::madCompress(hash, 10007, a, b, p) ==
                referenceMad(hash, a, b, p, 10007), "MAD differs from oracle.");
    }
}

void testThousandsOfKeys() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(11, mode);
        fill(table, 5000);
        for (int i = 0; i < 5000; ++i) {
            checkBook(table.findByISBN(std::to_string(i)), makeBook(std::to_string(i), i % 2 == 0));
        }
        for (int i = 0; i < 5000; ++i) {
            require(table.remove(std::to_string(i)), "Bulk removal.");
        }
        require(table.size() == 0 && table.capacity() == 11, "Bulk final state.");
    }
}

void testInvalidParameters() {
    expectThrows<std::invalid_argument>([] { SeparateChainingHash table(0); });
    expectThrows<std::invalid_argument>([] { SeparateChainingHash table(11, static_cast<CompressionMode>(99)); });
    expectThrows<std::invalid_argument>([] { HashUtils::moduloCompress(10, 0); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 0, 1, 0, 13); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 13, 1, 0, 13); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 17, 1, 0, 13); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 5, 1, 0, 9); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 5, 0, 0, 13); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 5, 13, 0, 13); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 5, 1, 13, 13); });
    expectThrows<std::invalid_argument>([] { HashUtils::madCompress(10, 1, 1, 0, 1); });
    expectThrows<std::invalid_argument>([] {
        HashUtils::madCompress(10, 5, 1, 0, std::numeric_limits<std::uint64_t>::max());
    });
    expectThrows<std::invalid_argument>([] {
        SeparateChainingHash table(static_cast<std::size_t>(HashUtils::defaultMadPrime), CompressionMode::MAD);
    });
}

void testRepeatedRehash() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(3, mode);
        for (int round = 0; round < 8; ++round) {
            fill(table, 120);
            for (int i = 0; i < 120; ++i) {
                require(!table.insert(makeBook(std::to_string(i))), "Repeated duplicate.");
                require(table.remove(std::to_string(i)), "Repeated removal.");
            }
            require(table.size() == 0 && table.capacity() == 3, "Repeated rehash state.");
        }
    }
}

void testShrinkPrimeRules() {
    require(HashUtils::shrinkPrimeCapacity(23, 3) == 11, "Half 11.5.");
    require(HashUtils::shrinkPrimeCapacity(17, 3) == 7, "Half 8.5.");
    require(HashUtils::shrinkPrimeCapacity(12, 2) == 5, "Tie must choose five.");
    require(HashUtils::shrinkPrimeCapacity(11, 7) == 7, "Minimum clamp.");
    require(HashUtils::shrinkPrimeCapacity(11, 11) == 11, "No shrink.");
    expectThrows<std::invalid_argument>([] { HashUtils::shrinkPrimeCapacity(11, 4); });
    expectThrows<std::invalid_argument>([] { HashUtils::shrinkPrimeCapacity(3, 5); });
    for (std::size_t current = 3; current < 200; ++current) {
        const auto chosen = HashUtils::shrinkPrimeCapacity(current, 2);
        const auto distance = [current](std::size_t prime) {
            return prime * 2 > current ? prime * 2 - current : current - prime * 2;
        };
        for (std::size_t prime = 2; prime < current; ++prime) {
            if (HashUtils::isPrime(prime)) {
                require(distance(chosen) <= distance(prime), "Not nearest prime.");
                if (distance(chosen) == distance(prime)) require(chosen <= prime, "Tie rule.");
            }
        }
    }
}

void testUnsignedAndOverflow() {
    const std::string bytes(1, static_cast<char>(0xFF));
    require(HashUtils::polynomialHash(bytes) == 255, "Signed character.");
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    require(HashUtils::polynomialHash("AB", maximum) == 1, "Unsigned wraparound.");
    require(HashUtils::polynomialHash("ABC", 0) == 67, "Base zero convention.");
    expectThrows<std::overflow_error>([] {
        HashUtils::nextPrime(std::numeric_limits<std::size_t>::max());
    });
    expectThrows<std::overflow_error>([] {
        HashUtils::nextPrime(std::numeric_limits<std::size_t>::max() - 1);
    });
}

void testChainRemovalPositions() {
    SeparateChainingHash table(5, CompressionMode::MODULO, 1, false);
    for (const auto* key : {"AB", "BA", "C@", "D?"}) table.insert(makeBook(key));
    require(table.remove("BA"), "Middle removal.");
    checkBook(table.findByISBN("C@"), makeBook("C@"));
    require(table.remove("AB"), "Head removal.");
    require(table.remove("D?"), "Tail removal.");
    checkBook(table.findByISBN("C@"), makeBook("C@"));
    require(table.remove("C@") && table.maxChainLength() == 0, "Last removal.");
}

void testRehashCollisionAccounting() {
    SeparateChainingHash table(3, CompressionMode::MODULO, 1);
    for (const auto* key : {"A", "D", "G"}) table.insert(makeBook(key));
    require(table.capacity() == 7 && table.collisionCount() == 2, "Growth counted.");
    table.remove("D");
    table.remove("G");
    require(table.capacity() == 3 && table.collisionCount() == 2, "Shrink counted.");
    checkBook(table.findByISBN("A"), makeBook("A"));
}

void testTextKeys() {
    SeparateChainingHash table;
    for (const auto& key : {std::string(), std::string("001"), std::string("1"),
                           std::string("a\0b", 3), std::string(10000, 'x')}) {
        require(table.insert(makeBook(key)), "Text key rejected.");
        checkBook(table.findByISBN(key), makeBook(key));
        require(!table.insert(makeBook(key)), "Text duplicate.");
    }
    require(table.size() == 5, "Text key identity.");
}

void testRandomizedReference() {
    for (auto mode : {CompressionMode::MODULO, CompressionMode::MAD}) {
        SeparateChainingHash table(3, mode);
        std::map<std::string, Book> reference; // Ordered tree, not a hash table.
        std::mt19937 random(42);
        for (int step = 0; step < 6000; ++step) {
            const auto key = std::to_string(random() % 400);
            const Book book = makeBook(key, step % 2 == 0);
            switch (random() % 3) {
            case 0:
                require(table.insert(book) == reference.emplace(key, book).second, "Reference insert.");
                break;
            case 1:
                require(table.remove(key) == (reference.erase(key) != 0), "Reference removal.");
                break;
            default: {
                const auto found = reference.find(key);
                if (found == reference.end()) require(!table.findByISBN(key), "Reference missing.");
                else checkBook(table.findByISBN(key), found->second);
                break;
            }
            }
            require(table.size() == reference.size(), "Reference size.");
        }
        for (const auto& entry : reference) checkBook(table.findByISBN(entry.first), entry.second);
    }
}

struct TestCase {
    const char* name;
    void (*run)();
};
}

int main() {
    const TestCase cases[] = {
        {"empty table", testEmpty},
        {"successful insertion", testInsertion},
        {"duplicate rejection", testDuplicate},
        {"successful lookup", testLookup},
        {"missing lookup", testMissingLookup},
        {"successful removal", testRemoval},
        {"missing removal", testMissingRemoval},
        {"load factor", testLoadFactor},
        {"modulo compression", testModulo},
        {"MAD compression", testMad},
        {"polynomial base 1", testBaseOne},
        {"polynomial base 37", testBaseThirtySeven},
        {"primality", testPrimes},
        {"strict nextPrime", testNextPrime},
        {"forced collisions", testForcedCollisions},
        {"collision counter", testCollisionCounter},
        {"maximum chain", testMaximumChain},
        {"expansion threshold", testExpansionThreshold},
        {"expansion prime", testExpansionPrime},
        {"expansion preservation", testExpansionPreservesBooks},
        {"shrink threshold", testShrinkThreshold},
        {"shrink preservation", testShrinkPreservesBooks},
        {"minimum capacity", testMinimumCapacity},
        {"fixed capacity", testFixedCapacity},
        {"both compression modes", testBothModes},
        {"search probes", testProbes},
        {"large safe MAD", testLargeMad},
        {"thousands of unique keys", testThousandsOfKeys},
        {"invalid arguments", testInvalidParameters},
        {"repeated rehash", testRepeatedRehash},
        {"shrink nearest prime and ties", testShrinkPrimeRules},
        {"unsigned hashing and overflow", testUnsignedAndOverflow},
        {"chain removal positions", testChainRemovalPositions},
        {"rehash collision accounting", testRehashCollisionAccounting},
        {"textual and long keys", testTextKeys},
        {"seeded reference comparison", testRandomizedReference}
    };
    int failures = 0;
    for (const auto& test : cases) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }
    const auto count = sizeof(cases) / sizeof(cases[0]);
    std::cout << count - static_cast<std::size_t>(failures) << '/' << count << " tests passed.\n";
    return failures == 0 ? 0 : 1;
}

