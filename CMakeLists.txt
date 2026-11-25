cmake_minimum_required(VERSION 3.16)
project(VSA1_Labwork4 LANGUAGES CXX)

# Keep accidental in-source configuration from producing build files here.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use cmake -S . -B build")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Enable AddressSanitizer (and UBSan on GCC/Clang)" OFF)

function(configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
            target_link_options(${target} PRIVATE /INCREMENTAL:NO)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
        if(ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_library(chaining src/HashUtils.cpp src/SeparateChainingHash.cpp src/OpenAddressingHash.cpp)
target_include_directories(chaining PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
configure_target(chaining)

add_executable(library_catalogue main.cpp)
target_link_libraries(library_catalogue PRIVATE chaining)
configure_target(library_catalogue)

add_executable(test_chaining tests/test_chaining.cpp)
target_link_libraries(test_chaining PRIVATE chaining)
configure_target(test_chaining)

enable_testing()
add_test(NAME task1_chaining COMMAND test_chaining)

add_executable(test_open_addressing tests/test_open_addressing.cpp)
target_link_libraries(test_open_addressing PRIVATE chaining)
configure_target(test_open_addressing)
add_test(NAME task2_open_addressing COMMAND test_open_addressing)

add_library(benchmark_support benchmarks/BenchmarkUtils.cpp)
target_link_libraries(benchmark_support PUBLIC chaining)
configure_target(benchmark_support)

add_executable(benchmark benchmarks/benchmark.cpp)
target_link_libraries(benchmark PRIVATE benchmark_support)
configure_target(benchmark)

add_executable(test_benchmark tests/test_benchmark.cpp)
target_link_libraries(test_benchmark PRIVATE benchmark_support)
configure_target(test_benchmark)
add_test(NAME task3_benchmark_correctness COMMAND test_benchmark)
