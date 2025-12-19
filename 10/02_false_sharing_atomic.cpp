/*
 * 02_false_sharing_atomic.cpp
 * 
 * CONCEPT: False Sharing Mitigation
 * TECHNIQUE: False Sharing with atomics
 * 
 * This program demonstrates false sharing - a performance problem that occurs
 * when multiple threads modify different variables that happen to share the
 * same CPU cache line (typically 64 bytes).
 * 
 * Even though threads are updating DIFFERENT variables, the cache coherence
 * protocol (MESI) treats the entire cache line as a unit, causing:
 *   - Cache line invalidations on every write
 *   - Cache line transfers between cores (ping-pong effect)
 *   - Dramatic performance degradation
 * 
 * SOLUTION: Use alignas(64) or padding to ensure each variable is on its
 * own cache line, eliminating the false sharing.
 * 
 * COMPILATION:
 *   g++ -O3 -pthread -o 02_false_sharing_atomic 02_false_sharing_atomic.cpp
 * 
 * RUN:
 *   ./02_false_sharing_atomic
 */

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

// Cache line size on modern x86 CPUs
constexpr size_t CACHE_LINE_SIZE = 64;

// Number of increments per thread
constexpr long long ITERATIONS = 100'000'000;

/*
 * BAD: Two atomics on the SAME cache line
 * 
 * sizeof(std::atomic<long long>) is typically 8 bytes.
 * Both 'a' and 'b' fit within a single 64-byte cache line.
 * When thread 1 updates 'a', it invalidates the cache line
 * containing 'b', forcing thread 2 to re-fetch it.
 */
struct FalseSharing {
    std::atomic<long long> a{0};  // 8 bytes
    std::atomic<long long> b{0};  // 8 bytes, same cache line as 'a'
};

/*
 * GOOD: Each atomic on its OWN cache line
 * 
 * Using alignas(64) forces each variable to start at a 64-byte boundary.
 * This ensures 'a' and 'b' are on different cache lines.
 * Updates to 'a' no longer invalidate the cache line containing 'b'.
 */
struct NoFalseSharing {
    alignas(CACHE_LINE_SIZE) std::atomic<long long> a{0};  // Own cache line
    alignas(CACHE_LINE_SIZE) std::atomic<long long> b{0};  // Own cache line
};

/*
 * Alternative: Use padding to separate variables
 * 
 * This achieves the same effect as alignas but is more portable
 * and works even when alignas isn't fully supported.
 */
struct PaddedNoFalseSharing {
    std::atomic<long long> a{0};
    char padding[CACHE_LINE_SIZE - sizeof(std::atomic<long long>)];  // Padding
    std::atomic<long long> b{0};
};

/*
 * Worker function that increments a given atomic variable
 */
void increment_worker(std::atomic<long long>& counter, long long iterations) {
    for (long long i = 0; i < iterations; ++i) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

/*
 * Benchmark a structure with two threads updating different atomics
 */
template<typename T>
double benchmark_structure(T& data) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Thread 1 updates 'a', Thread 2 updates 'b'
    std::thread t1(increment_worker, std::ref(data.a), ITERATIONS);
    std::thread t2(increment_worker, std::ref(data.b), ITERATIONS);
    
    t1.join();
    t2.join();
    
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int main() {
    std::cout << "=== False Sharing Demonstration with Atomics ===\n\n";
    
    std::cout << "Configuration:\n";
    std::cout << "  Cache line size: " << CACHE_LINE_SIZE << " bytes\n";
    std::cout << "  sizeof(std::atomic<long long>): " << sizeof(std::atomic<long long>) << " bytes\n";
    std::cout << "  Iterations per thread: " << ITERATIONS << "\n";
    std::cout << "  Hardware threads: " << std::thread::hardware_concurrency() << "\n\n";
    
    // Show structure sizes
    std::cout << "Structure Sizes:\n";
    std::cout << "  FalseSharing:         " << sizeof(FalseSharing) << " bytes\n";
    std::cout << "  NoFalseSharing:       " << sizeof(NoFalseSharing) << " bytes\n";
    std::cout << "  PaddedNoFalseSharing: " << sizeof(PaddedNoFalseSharing) << " bytes\n\n";
    
    // Run benchmarks multiple times for stability
    const int RUNS = 3;
    double false_sharing_total = 0;
    double no_false_sharing_total = 0;
    double padded_total = 0;
    
    for (int run = 0; run < RUNS; ++run) {
        std::cout << "Run " << (run + 1) << ":\n";
        
        // Test with false sharing
        FalseSharing fs;
        double fs_time = benchmark_structure(fs);
        false_sharing_total += fs_time;
        std::cout << "  False Sharing:     " << std::fixed << std::setprecision(2) 
                  << fs_time << " ms (a=" << fs.a << ", b=" << fs.b << ")\n";
        
        // Test without false sharing (alignas)
        NoFalseSharing nfs;
        double nfs_time = benchmark_structure(nfs);
        no_false_sharing_total += nfs_time;
        std::cout << "  No False Sharing:  " << nfs_time << " ms (a=" << nfs.a << ", b=" << nfs.b << ")\n";
        
        // Test without false sharing (padding)
        PaddedNoFalseSharing pnfs;
        double pnfs_time = benchmark_structure(pnfs);
        padded_total += pnfs_time;
        std::cout << "  Padded:            " << pnfs_time << " ms (a=" << pnfs.a << ", b=" << pnfs.b << ")\n\n";
    }
    
    // Average results
    std::cout << "=== Average Results ===\n";
    double avg_fs = false_sharing_total / RUNS;
    double avg_nfs = no_false_sharing_total / RUNS;
    double avg_padded = padded_total / RUNS;
    
    std::cout << "False Sharing:     " << avg_fs << " ms\n";
    std::cout << "No False Sharing:  " << avg_nfs << " ms\n";
    std::cout << "Padded:            " << avg_padded << " ms\n\n";
    
    std::cout << "Speedup (avoiding false sharing): " << avg_fs / avg_nfs << "x\n\n";
    
    std::cout << "=== What's Happening ===\n";
    std::cout << "WITH False Sharing:\n";
    std::cout << "  1. Thread 1 writes to 'a', marking cache line as Modified\n";
    std::cout << "  2. Thread 2 writes to 'b' (same cache line!)\n";
    std::cout << "  3. Thread 2 must first invalidate Thread 1's copy\n";
    std::cout << "  4. Thread 2 fetches the cache line, modifies 'b'\n";
    std::cout << "  5. Thread 1's next write must re-fetch... (ping-pong!)\n\n";
    
    std::cout << "WITHOUT False Sharing:\n";
    std::cout << "  1. Thread 1 writes to 'a' on cache line 1\n";
    std::cout << "  2. Thread 2 writes to 'b' on cache line 2\n";
    std::cout << "  3. No interference - each core owns its cache line\n\n";
    
    std::cout << "=== Recommendations ===\n";
    std::cout << "1. Use alignas(" << CACHE_LINE_SIZE << ") for variables accessed by different threads\n";
    std::cout << "2. Group thread-local data together in cache-line-sized chunks\n";
    std::cout << "3. Consider using std::hardware_destructive_interference_size (C++17)\n";
    std::cout << "4. Profile with tools like perf or VTune to detect cache issues\n";
    
    return 0;
}
