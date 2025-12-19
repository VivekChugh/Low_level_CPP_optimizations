/**
 * Arithmetic vs. Memory Cost Demo
 * 
 * This code demonstrates "The Real Cost Model" - showing that memory access
 * is often far more expensive than arithmetic operations.
 * 
 * Key concepts:
 * - Cache misses are extremely expensive (100+ cycles)
 * - Arithmetic operations are cheap (1-5 cycles)
 * - Random memory access defeats CPU prefetchers
 * - Sequential access leverages cache and prefetching
 * 
 * The demo compares:
 * 1. A loop with many arithmetic operations but no memory access
 * 2. A loop with simple operations but random memory access (cache misses)
 * 3. A loop with sequential memory access (cache friendly)
 * 
 * Compile: g++ -O3 -std=c++17 -o 02_arithmetic_vs_memory_cost 02_arithmetic_vs_memory_cost.cpp
 * Run: ./02_arithmetic_vs_memory_cost
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <numeric>
#include <iomanip>

// Timer class for measuring execution time
class Timer {
    using Clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<Clock> start;
public:
    Timer() : start(Clock::now()) {}
    void reset() { start = Clock::now(); }
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
};

// Prevent compiler from optimizing away results
volatile long long sink;

// Test 1: Heavy arithmetic, minimal memory access
// This loop performs many arithmetic operations per iteration
// but only touches a small amount of memory (fits in registers/L1 cache)
long long heavy_arithmetic(size_t iterations) {
    long long a = 1, b = 2, c = 3, d = 4;
    
    for (size_t i = 0; i < iterations; ++i) {
        // Perform many arithmetic operations
        // These can execute in parallel on modern CPUs (instruction-level parallelism)
        a = a + b * c - d;
        b = b * 3 + a - c;
        c = c + d * 2 + a;
        d = d - a + b * c;
        
        // More operations to increase arithmetic intensity
        a = (a ^ b) + (c & d);
        b = (b | c) - (d ^ a);
        c = (c * 7) + (a / 3 + 1);  // +1 to avoid potential div-by-zero
        d = (d + 11) * (b % 17 + 1);
    }
    
    return a + b + c + d;  // Return something to prevent dead code elimination
}

// Test 2: Random memory access (cache unfriendly)
// This simulates pointer chasing or random data access patterns
// Each access likely causes a cache miss
long long random_memory_access(const std::vector<size_t>& indices, 
                                const std::vector<long long>& data) {
    long long sum = 0;
    
    for (size_t idx : indices) {
        // Random access into data array
        // The CPU cannot predict where we'll read next
        // This defeats the hardware prefetcher
        sum += data[idx];
    }
    
    return sum;
}

// Test 3: Sequential memory access (cache friendly)
// This demonstrates the power of spatial locality
// The prefetcher can predict and load upcoming cache lines
long long sequential_memory_access(const std::vector<long long>& data) {
    long long sum = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        // Sequential access - each element is adjacent in memory
        // CPU prefetcher loads upcoming cache lines in advance
        // Almost every access hits the cache
        sum += data[i];
    }
    
    return sum;
}

int main() {
    std::cout << "=== Arithmetic vs. Memory Cost Demo ===\n";
    std::cout << "Demonstrating that memory access often dominates performance\n\n";
    
    // Configuration
    const size_t N = 10000000;  // 10 million iterations/elements
    const int num_runs = 5;
    
    // Prepare data for memory tests
    std::vector<long long> data(N);
    std::vector<size_t> random_indices(N);
    
    // Initialize data with some values
    std::iota(data.begin(), data.end(), 0);  // Fill with 0, 1, 2, ...
    
    // Create random access pattern
    // This shuffles indices so we access memory in random order
    std::iota(random_indices.begin(), random_indices.end(), 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(random_indices.begin(), random_indices.end(), gen);
    
    std::cout << "Array size: " << N << " elements ("
              << (N * sizeof(long long)) / (1024.0 * 1024.0) << " MB)\n\n";
    
    // Warm-up runs
    std::cout << "Warming up...\n";
    sink = heavy_arithmetic(N / 10);
    sink = random_memory_access(
        std::vector<size_t>(random_indices.begin(), random_indices.begin() + N/10),
        data);
    sink = sequential_memory_access(
        std::vector<long long>(data.begin(), data.begin() + N/10));
    
    // Test 1: Heavy Arithmetic
    std::cout << "\n--- Test 1: Heavy Arithmetic (minimal memory access) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = heavy_arithmetic(N);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        double avg_time = total_time / num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "Operations per iteration: ~16 arithmetic ops\n";
        std::cout << "Total operations: ~" << (N * 16) / 1000000 << " million\n";
    }
    
    // Test 2: Random Memory Access
    std::cout << "\n--- Test 2: Random Memory Access (cache unfriendly) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = random_memory_access(random_indices, data);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        double avg_time = total_time / num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "This is slow because each random access likely misses cache\n";
        std::cout << "Cache miss penalty: ~100-300 CPU cycles per miss\n";
    }
    
    // Test 3: Sequential Memory Access
    std::cout << "\n--- Test 3: Sequential Memory Access (cache friendly) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = sequential_memory_access(data);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        double avg_time = total_time / num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "This is fast because prefetcher loads data in advance\n";
        std::cout << "Sequential access leverages spatial locality\n";
    }
    
    // Summary
    std::cout << "\n=== Key Takeaways ===\n";
    std::cout << "1. Arithmetic operations are extremely cheap on modern CPUs\n";
    std::cout << "2. Random memory access is expensive due to cache misses\n";
    std::cout << "3. Sequential access is fast due to prefetching and locality\n";
    std::cout << "4. Optimizing memory access patterns often yields bigger gains\n";
    std::cout << "   than optimizing arithmetic operations\n";
    
    return 0;
}
