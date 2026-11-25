#include "BenchmarkUtils.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

int main(int argc, char* argv[]) {
    try {
        if (argc > 2) {
            std::cerr << "Usage: benchmark [output-directory]\n";
            return 1;
        }
        const std::filesystem::path directory = argc == 2 ? argv[1] : "results";
        const auto keys = Benchmark::generateKeys();
        const auto missing = Benchmark::generateMissingKeys(1000, keys);
        std::vector<Benchmark::DistributionResult> polynomial, compression;
        for (const std::uint64_t base : {1ULL, 37ULL}) {
            const auto modulo = Benchmark::measureDistribution(
                keys, Benchmark::defaultCapacity, base, CompressionMode::MODULO);
            polynomial.push_back(modulo);
            compression.push_back(modulo);
            compression.push_back(Benchmark::measureDistribution(
                keys, Benchmark::defaultCapacity, base, CompressionMode::MAD));
        }
        std::vector<Benchmark::ArchitectureResult> architecture;
        for (double load : {0.25, 0.50, 0.75, 0.90}) {
            const auto rows = Benchmark::compareArchitectures(
                keys, missing, Benchmark::defaultCapacity, load);
            architecture.insert(architecture.end(), rows.begin(), rows.end());
        }
        std::filesystem::create_directories(directory);
        std::ofstream polynomialFile(directory / "polynomial_collisions.csv");
        std::ofstream compressionFile(directory / "compression_comparison.csv");
        std::ofstream loadFile(directory / "load_factor_comparison.csv");
        Benchmark::writePolynomialCsv(polynomialFile, polynomial);
        Benchmark::writeCompressionCsv(compressionFile, compression);
        Benchmark::writeLoadCsv(loadFile, architecture);
        polynomialFile.close(); compressionFile.close(); loadFile.close();
        if (!polynomialFile || !compressionFile || !loadFile) {
            throw std::runtime_error("Could not finish writing CSV files.");
        }
        std::cout << "Dataset: " << keys.size() << " unique ISBN-like keys; seed=42; N=10007\n";
        for (const auto& row : compression) {
            std::cout << "base=" << row.base << ' ' << row.compression
                      << " raw collisions=" << row.rawCollisions
                      << " index collisions=" << row.indexCollisions
                      << " occupied=" << row.occupiedBuckets
                      << " max bucket=" << row.maxBucketOccupancy << '\n';
        }
        std::cout << std::fixed << std::setprecision(4);
        for (const auto& row : architecture) {
            std::cout << row.architecture << " target=" << row.targetLoad
                      << " actual=" << row.actualLoad << " inserted=" << row.successful
                      << '/' << row.attempted << " failed=" << row.failed
                      << " achieved=" << (row.targetAchieved ? "yes" : "no")
                      << " max=" << row.maxLength
                      << " hit probes=" << Benchmark::average(row.successSearch.probes, row.successSearch.queries)
                      << " miss probes=" << Benchmark::average(row.missingSearch.probes, row.missingSearch.queries)
                      << '\n';
        }
        std::cout << "Chaining probes = key comparisons; OA probes = inspected slots.\n"
                     "Common-key comparisons and totals are included in CSV.\n"
                  << "CSV output: " << directory.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 1;
    }
}
