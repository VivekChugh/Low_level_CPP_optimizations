/**
 * False Sharing Prevention
 * 
 * This code demonstrates false sharing - a performance killer in multi-threaded
 * code where threads modify different variables that happen to share a cache line.
 * 
 * Key concepts:
 * - Cache coherence: When one core modifies data, other cores' copies are invalidated
 * - False sharing: Two threads modify different variables on the same cache line
 * - Each modification causes the cache line to "ping-pong" between cores
 * - Solution: Pad data so each thread's variable is on its own cache line
 * 
 * This demo compares:
 * 1. Bad: Two variables sharing a cache line (false sharing)
 * 2. Good: Variables padded to separate cache lines (no false sharing)
 * 
 * Expected result: Padded version is significantly faster (2x-10x)
 * 
 * Compile: g++ -O3 -std=c++17 -pthread -o 03_false_sharing 03_false_sharing.cpp
 * Run: ./03_false_sharing
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <vector>

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

// Cache line size on most modern x86 processors
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * BAD: Two counters sharing a cache line
 * 
 * Memory layout (assuming 8-byte long long):
 * [  counter_a (8 bytes)  ][  counter_b (8 bytes)  ][...padding...]
 * |<---------------------- 64 byte cache line -------------------->|
 * 
 * When Thread 1 writes to counter_a:
 * - The entire cache line is marked "modified" on Core 1
 * - Core 2's copy of the cache line is invalidated
 * - Core 2 must fetch the line again to access counter_b
 * 
 * When Thread 2 writes to counter_b:
 * - Same thing happens in reverse
 * - Cache line ping-pongs between cores
 */
struct SharedCacheLine {
    long long counter_a;  // Thread 1 will modify this
    long long counter_b;  // Thread 2 will modify this
    // Both are on the same 64-byte cache line!
};

/**
 * GOOD: Each counter on its own cache line
 * 
 * Memory layout with padding:
 * [counter_a (8 bytes)][padding (56 bytes)]|[counter_b (8 bytes)][padding (56 bytes)]
 * |<------- cache line 1 --------------->||<------- cache line 2 --------------->|
 * 
 * Now each thread modifies a different cache line.
 * No invalidation, no ping-pong, no false sharing!
 */
struct alignas(CACHE_LINE_SIZE) PaddedCounter {
    long long value;
    // The alignas(64) ensures this struct starts at a 64-byte boundary
    // and the struct size is padded to 64 bytes
};

struct SeparateCacheLines {
    PaddedCounter counter_a;  // On its own cache line
    PaddedCounter counter_b;  // On its own cache line
};

/**
 * Alternative using explicit padding
 * This is more portable and makes the padding explicit
 */
struct ExplicitlyPadded {
    long long counter_a;
    char padding1[CACHE_LINE_SIZE - sizeof(long long)];  // Pad to cache line
    long long counter_b;
    char padding2[CACHE_LINE_SIZE - sizeof(long long)];  // Pad to cache line
};

// Number of iterations for each thread
constexpr long long ITERATIONS = 100000000;  // 100 million

/**
 * Worker function that increments a counter many times
 * The pointer allows us to target different memory locations
 */
void increment_counter(long long* counter, long long iterations) {
    for (long long i = 0; i < iterations; ++i) {
        // Increment the counter
        // Each write potentially invalidates another core's cache line
        (*counter)++;
    }
}

/**
 * Test false sharing with shared cache line
 */
double test_false_sharing() {
    SharedCacheLine data{0, 0};
    
    Timer timer;
    
    // Launch two threads, each modifying a different counter
    // But both counters are on the SAME cache line!
    std::thread t1(increment_counter, &data.counter_a, ITERATIONS);
    std::thread t2(increment_counter, &data.counter_b, ITERATIONS);
    
    t1.join();
    t2.join();
    
    double elapsed = timer.elapsed_ms();
    
    // Verify results
    if (data.counter_a != ITERATIONS || data.counter_b != ITERATIONS) {
        std::cerr << "ERROR: Unexpected counter values!\n";
    }
    
    return elapsed;
}

/**
 * Test with padded counters (no false sharing)
 */
double test_no_false_sharing() {
    SeparateCacheLines data;
    data.counter_a.value = 0;
    data.counter_b.value = 0;
    
    Timer timer;
    
    // Launch two threads, each modifying a counter on its OWN cache line
    // No cache line sharing = no false sharing!
    std::thread t1(increment_counter, &data.counter_a.value, ITERATIONS);
    std::thread t2(increment_counter, &data.counter_b.value, ITERATIONS);
    
    t1.join();
    t2.join();
    
    double elapsed = timer.elapsed_ms();
    
    // Verify results
    if (data.counter_a.value != ITERATIONS || data.counter_b.value != ITERATIONS) {
        std::cerr << "ERROR: Unexpected counter values!\n";
    }
    
    return elapsed;
}

/**
 * Baseline: Single-threaded performance
 */
double test_single_threaded() {
    long long counter = 0;
    
    Timer timer;
    
    // Single thread, double the work
    increment_counter(&counter, ITERATIONS * 2);
    
    double elapsed = timer.elapsed_ms();
    
    if (counter != ITERATIONS * 2) {
        std::cerr << "ERROR: Unexpected counter value!\n";
    }
    
    return elapsed;
}

int main() {
    std::cout << "=== False Sharing Prevention Demo ===\n\n";
    
    // System info
    std::cout << "System configuration:\n";
    std::cout << "  Hardware threads: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "  Cache line size: " << CACHE_LINE_SIZE << " bytes\n";
    std::cout << "  Iterations per thread: " << ITERATIONS << "\n\n";
    
    // Show struct sizes
    std::cout << "Data structure sizes:\n";
    std::cout << "  SharedCacheLine: " << sizeof(SharedCacheLine) << " bytes\n";
    std::cout << "  SeparateCacheLines: " << sizeof(SeparateCacheLines) << " bytes\n";
    std::cout << "  PaddedCounter: " << sizeof(PaddedCounter) << " bytes\n";
    std::cout << "  ExplicitlyPadded: " << sizeof(ExplicitlyPadded) << " bytes\n\n";
    
    // Show memory layout
    std::cout << "Memory layout analysis:\n";
    SharedCacheLine shared;
    std::cout << "  SharedCacheLine:\n";
    std::cout << "    &counter_a: " << &shared.counter_a << "\n";
    std::cout << "    &counter_b: " << &shared.counter_b << "\n";
    std::cout << "    Distance: " << ((char*)&shared.counter_b - (char*)&shared.counter_a) << " bytes\n";
    std::cout << "    Same cache line? YES (both within 64 bytes)\n\n";
    
    SeparateCacheLines separate;
    std::cout << "  SeparateCacheLines:\n";
    std::cout << "    &counter_a: " << &separate.counter_a.value << "\n";
    std::cout << "    &counter_b: " << &separate.counter_b.value << "\n";
    std::cout << "    Distance: " << ((char*)&separate.counter_b.value - (char*)&separate.counter_a.value) << " bytes\n";
    std::cout << "    Same cache line? NO (>= 64 bytes apart)\n\n";
    
    const int num_runs = 5;
    
    // Test single-threaded baseline
    std::cout << "--- Single-threaded baseline ---\n";
    double single_total = 0;
    for (int i = 0; i < num_runs; ++i) {
        double time = test_single_threaded();
        single_total += time;
        std::cout << "  Run " << (i + 1) << ": " << std::fixed << std::setprecision(2) << time << " ms\n";
    }
    double single_avg = single_total / num_runs;
    std::cout << "Average: " << single_avg << " ms\n\n";
    
    // Test with false sharing
    std::cout << "--- With FALSE SHARING (bad) ---\n";
    double false_sharing_total = 0;
    for (int i = 0; i < num_runs; ++i) {
        double time = test_false_sharing();
        false_sharing_total += time;
        std::cout << "  Run " << (i + 1) << ": " << std::fixed << std::setprecision(2) << time << " ms\n";
    }
    double false_sharing_avg = false_sharing_total / num_runs;
    std::cout << "Average: " << false_sharing_avg << " ms\n\n";
    
    // Test without false sharing
    std::cout << "--- WITHOUT false sharing (good) ---\n";
    double no_false_sharing_total = 0;
    for (int i = 0; i < num_runs; ++i) {
        double time = test_no_false_sharing();
        no_false_sharing_total += time;
        std::cout << "  Run " << (i + 1) << ": " << std::fixed << std::setprecision(2) << time << " ms\n";
    }
    double no_false_sharing_avg = no_false_sharing_total / num_runs;
    std::cout << "Average: " << no_false_sharing_avg << " ms\n\n";
    
    // Summary
    std::cout << "=== Results Summary ===\n";
    std::cout << "Single-threaded (2x work): " << std::fixed << std::setprecision(2) 
              << single_avg << " ms\n";
    std::cout << "Two threads WITH false sharing: " << false_sharing_avg << " ms\n";
    std::cout << "Two threads WITHOUT false sharing: " << no_false_sharing_avg << " ms\n";
    std::cout << "\nSpeedup from fixing false sharing: " << std::setprecision(1) 
              << (false_sharing_avg / no_false_sharing_avg) << "x\n";
    
    // Expected parallelism
    double expected_parallel = single_avg / 2.0;  // Ideal would be half the time
    std::cout << "\nParallel efficiency analysis:\n";
    std::cout << "  Ideal parallel time (single/2): " << std::setprecision(2) << expected_parallel << " ms\n";
    std::cout << "  With false sharing: " << std::setprecision(0) 
              << (false_sharing_avg / expected_parallel * 100) << "% of ideal\n";
    std::cout << "  Without false sharing: " 
              << (no_false_sharing_avg / expected_parallel * 100) << "% of ideal\n";
    
    // Explanation
    std::cout << "\n=== What's Happening Under the Hood ===\n\n";
    
    std::cout << "FALSE SHARING (bad case):\n";
    std::cout << "1. Thread 1 on Core 1 writes to counter_a\n";
    std::cout << "2. Core 1 marks cache line as 'Modified'\n";
    std::cout << "3. Core 2's copy is invalidated (MESI protocol)\n";
    std::cout << "4. Thread 2 on Core 2 writes to counter_b\n";
    std::cout << "5. Core 2 must fetch the line from Core 1 (~100 cycles)\n";
    std::cout << "6. Core 2 marks line as 'Modified'\n";
    std::cout << "7. Core 1's copy is invalidated\n";
    std::cout << "8. Repeat millions of times = cache line ping-pong!\n\n";
    
    std::cout << "NO FALSE SHARING (good case):\n";
    std::cout << "1. Thread 1 writes to counter_a on cache line A\n";
    std::cout << "2. Thread 2 writes to counter_b on cache line B\n";
    std::cout << "3. No conflict - each core owns its cache line\n";
    std::cout << "4. Both threads run at full speed!\n\n";
    
    std::cout << "=== How to Prevent False Sharing ===\n";
    std::cout << "1. Use alignas(64) to align structures to cache lines\n";
    std::cout << "2. Add explicit padding between thread-local variables\n";
    std::cout << "3. Use std::hardware_destructive_interference_size (C++17)\n";
    std::cout << "4. Keep frequently-modified data on separate cache lines\n";
    std::cout << "5. Use thread-local storage when possible\n";
    
    return 0;
}
