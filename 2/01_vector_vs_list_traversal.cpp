/**
 * Locality: Vector vs. List Traversal
 * 
 * This code demonstrates the impact of spatial and temporal locality on performance.
 * It compares traversing elements in std::vector (contiguous memory) versus
 * std::list (non-contiguous memory with pointer chasing).
 * 
 * Key concepts:
 * - Spatial locality: Accessing nearby memory locations benefits from cache lines
 * - Temporal locality: Recently accessed data stays in cache
 * - std::vector stores elements contiguously - excellent cache utilization
 * - std::list stores elements scattered in heap - poor cache utilization
 * - Hardware prefetchers can predict sequential access patterns
 * 
 * Expected result: Vector traversal is 5x-20x faster than list traversal
 * 
 * Compile: g++ -O3 -std=c++17 -o 01_vector_vs_list 01_vector_vs_list_traversal.cpp
 * Run: ./01_vector_vs_list
 */

#include <iostream>
#include <vector>
#include <list>
#include <chrono>
#include <numeric>
#include <iomanip>
#include <random>

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

/**
 * Sum elements in a std::vector
 * 
 * Vector stores elements in contiguous memory:
 * [elem0][elem1][elem2][elem3]...
 * 
 * When we access elem0, the CPU loads an entire cache line (64 bytes).
 * This means elem1, elem2, etc. are likely already in cache!
 * The hardware prefetcher also detects the sequential pattern and
 * pre-loads upcoming cache lines before we need them.
 */
long long sum_vector(const std::vector<int>& vec) {
    long long sum = 0;
    
    // Sequential iteration through contiguous memory
    // Each access is likely a cache hit after the first one
    for (const auto& val : vec) {
        sum += val;
    }
    
    return sum;
}

/**
 * Sum elements in a std::list
 * 
 * List stores elements scattered across the heap:
 * [elem0] --ptr--> [elem1] --ptr--> [elem2] --ptr--> ...
 * 
 * Each element is in a separate memory location (node allocation).
 * To access the next element, we must:
 * 1. Follow a pointer (pointer chasing)
 * 2. Load memory from a potentially random location
 * 3. Likely suffer a cache miss each time
 * 
 * The prefetcher cannot predict where the next element is located.
 */
long long sum_list(const std::list<int>& lst) {
    long long sum = 0;
    
    // Each iteration involves pointer chasing
    // The next node could be anywhere in memory
    for (const auto& val : lst) {
        sum += val;  // Likely cache miss for each node
    }
    
    return sum;
}

/**
 * Alternative: Sum using iterators explicitly
 * Shows the pointer dereferencing more clearly
 */
long long sum_list_explicit(const std::list<int>& lst) {
    long long sum = 0;
    
    // Iterator traversal - each ++it follows a pointer
    for (auto it = lst.begin(); it != lst.end(); ++it) {
        // *it dereferences the iterator to get the value
        // ++it follows next pointer to the next node
        sum += *it;
    }
    
    return sum;
}

int main() {
    std::cout << "=== Vector vs. List Traversal: Locality Demo ===\n\n";
    
    // Configuration
    const size_t N = 10000000;  // 10 million elements
    const int num_runs = 5;
    
    std::cout << "Number of elements: " << N << "\n";
    std::cout << "Memory for vector: ~" << (N * sizeof(int)) / (1024.0 * 1024.0) << " MB (contiguous)\n";
    std::cout << "Memory for list: ~" << (N * (sizeof(int) + 2 * sizeof(void*))) / (1024.0 * 1024.0) 
              << " MB (scattered + overhead)\n\n";
    
    // Create and populate the vector
    std::cout << "Creating vector...\n";
    std::vector<int> vec(N);
    
    // Initialize with values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(1, 100);
    
    for (size_t i = 0; i < N; ++i) {
        vec[i] = dist(gen);
    }
    
    // Create and populate the list (this takes longer due to allocations)
    std::cout << "Creating list (slower due to individual allocations)...\n";
    std::list<int> lst(vec.begin(), vec.end());
    
    std::cout << "Data structures created.\n\n";
    
    // Warm-up runs
    std::cout << "Warming up caches...\n";
    sink = sum_vector(vec);
    sink = sum_list(lst);
    
    // Benchmark vector traversal
    std::cout << "\n--- Benchmarking Vector Traversal ---\n";
    double vector_total_time = 0;
    long long vector_result = 0;
    
    for (int run = 0; run < num_runs; ++run) {
        Timer timer;
        vector_result = sum_vector(vec);
        double elapsed = timer.elapsed_ms();
        vector_total_time += elapsed;
        sink = vector_result;
        std::cout << "  Run " << (run + 1) << ": " << std::fixed << std::setprecision(2) 
                  << elapsed << " ms\n";
    }
    
    double vector_avg = vector_total_time / num_runs;
    std::cout << "Vector average: " << vector_avg << " ms\n";
    std::cout << "Sum result: " << vector_result << "\n";
    
    // Benchmark list traversal
    std::cout << "\n--- Benchmarking List Traversal ---\n";
    double list_total_time = 0;
    long long list_result = 0;
    
    for (int run = 0; run < num_runs; ++run) {
        Timer timer;
        list_result = sum_list(lst);
        double elapsed = timer.elapsed_ms();
        list_total_time += elapsed;
        sink = list_result;
        std::cout << "  Run " << (run + 1) << ": " << std::fixed << std::setprecision(2) 
                  << elapsed << " ms\n";
    }
    
    double list_avg = list_total_time / num_runs;
    std::cout << "List average: " << list_avg << " ms\n";
    std::cout << "Sum result: " << list_result << "\n";
    
    // Verify results match
    if (vector_result != list_result) {
        std::cerr << "ERROR: Results don't match!\n";
        return 1;
    }
    
    // Summary
    std::cout << "\n=== Results Summary ===\n";
    std::cout << "Vector traversal: " << std::fixed << std::setprecision(2) 
              << vector_avg << " ms\n";
    std::cout << "List traversal:   " << list_avg << " ms\n";
    std::cout << "Speedup (vector vs list): " << std::setprecision(1) 
              << (list_avg / vector_avg) << "x faster\n";
    
    std::cout << "\n=== Why is Vector So Much Faster? ===\n";
    std::cout << "1. SPATIAL LOCALITY: Vector elements are contiguous.\n";
    std::cout << "   When one element is loaded, neighbors come free in the cache line.\n\n";
    std::cout << "2. PREFETCHING: CPU detects sequential access pattern.\n";
    std::cout << "   It pre-loads upcoming data before you need it.\n\n";
    std::cout << "3. NO POINTER CHASING: Vector access is direct indexing.\n";
    std::cout << "   List requires following pointers to scattered locations.\n\n";
    std::cout << "4. CACHE EFFICIENCY: Vector uses cache lines fully.\n";
    std::cout << "   List wastes cache space loading node overhead.\n";
    
    // Additional info about cache lines
    std::cout << "\n=== Cache Line Math ===\n";
    std::cout << "Typical cache line size: 64 bytes\n";
    std::cout << "Integers per cache line: " << (64 / sizeof(int)) << "\n";
    std::cout << "Vector: ~" << (N / (64 / sizeof(int))) << " cache line loads needed\n";
    std::cout << "List: ~" << N << " potential cache misses (each node separate)\n";
    
    return 0;
}
