/*
 * 03_memory_order.cpp
 * 
 * CONCEPT: Memory Order Cost
 * TECHNIQUE: Atomics and Memory Order
 * 
 * This program demonstrates the performance differences between various
 * memory ordering options in C++ atomics. Memory ordering determines:
 *   1. What synchronization guarantees are provided
 *   2. What compiler and CPU reorderings are allowed
 *   3. The performance cost of the atomic operation
 * 
 * Memory Orders (from weakest to strongest):
 *   - memory_order_relaxed: No ordering constraints (fastest)
 *   - memory_order_acquire: Prevents reads from moving before this load
 *   - memory_order_release: Prevents writes from moving after this store
 *   - memory_order_acq_rel: Combines acquire and release
 *   - memory_order_seq_cst: Total ordering across all threads (slowest)
 * 
 * COMPILATION:
 *   g++ -O3 -pthread -o 03_memory_order 03_memory_order.cpp
 * 
 * RUN:
 *   ./03_memory_order
 */

#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>

// Number of operations for each benchmark
constexpr long long ITERATIONS = 100'000'000;

/*
 * Prevent dead code elimination
 */
template<typename T>
void do_not_optimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

/*
 * Benchmark 1: Atomic counter with different memory orders
 * 
 * Demonstrates the cost difference between relaxed and sequential
 * consistency for a simple counter operation.
 */
void benchmark_counter() {
    std::cout << "=== Benchmark 1: Atomic Counter ===\n\n";
    
    // Relaxed ordering - fastest
    // No synchronization, just atomicity guaranteed
    {
        std::atomic<long long> counter{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < ITERATIONS; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "memory_order_relaxed: " << std::fixed << std::setprecision(2) 
                  << time_ms << " ms\n";
        do_not_optimize(counter);
    }
    
    // Sequential consistency - slowest
    // Full memory barrier, ensures total order
    {
        std::atomic<long long> counter{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < ITERATIONS; ++i) {
            counter.fetch_add(1, std::memory_order_seq_cst);  // Default
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "memory_order_seq_cst: " << time_ms << " ms\n\n";
        do_not_optimize(counter);
    }
}

/*
 * Benchmark 2: Flag-based signaling (producer-consumer pattern)
 * 
 * Demonstrates acquire-release semantics which are sufficient for
 * most synchronization patterns and cheaper than seq_cst.
 */
void benchmark_flag_signaling() {
    std::cout << "=== Benchmark 2: Flag Signaling (Load/Store) ===\n\n";
    
    // Test different memory orders for a simple flag check pattern
    const long long FLAG_ITERATIONS = ITERATIONS;
    
    // Relaxed load/store - fastest but may not synchronize correctly
    {
        std::atomic<bool> flag{false};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < FLAG_ITERATIONS; ++i) {
            flag.store(true, std::memory_order_relaxed);
            bool val = flag.load(std::memory_order_relaxed);
            do_not_optimize(val);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "memory_order_relaxed:  " << std::fixed << std::setprecision(2) 
                  << time_ms << " ms\n";
    }
    
    // Acquire-Release - provides synchronization without full barrier
    {
        std::atomic<bool> flag{false};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < FLAG_ITERATIONS; ++i) {
            flag.store(true, std::memory_order_release);
            bool val = flag.load(std::memory_order_acquire);
            do_not_optimize(val);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "memory_order_acq_rel:  " << time_ms << " ms\n";
    }
    
    // Sequential consistency - full memory barrier
    {
        std::atomic<bool> flag{false};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < FLAG_ITERATIONS; ++i) {
            flag.store(true, std::memory_order_seq_cst);
            bool val = flag.load(std::memory_order_seq_cst);
            do_not_optimize(val);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "memory_order_seq_cst:  " << time_ms << " ms\n\n";
    }
}

/*
 * Benchmark 3: Compare-and-swap (CAS) operations
 * 
 * CAS is the foundation of many lock-free algorithms.
 * Memory ordering affects how expensive the operation is.
 */
void benchmark_cas() {
    std::cout << "=== Benchmark 3: Compare-and-Swap (CAS) ===\n\n";
    
    const long long CAS_ITERATIONS = ITERATIONS / 2;  // CAS is more expensive
    
    // Relaxed CAS
    {
        std::atomic<long long> value{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < CAS_ITERATIONS; ++i) {
            long long expected = i;
            // compare_exchange_weak can fail spuriously but is faster
            value.compare_exchange_weak(expected, i + 1, 
                                       std::memory_order_relaxed,
                                       std::memory_order_relaxed);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "CAS (relaxed):  " << std::fixed << std::setprecision(2) 
                  << time_ms << " ms\n";
        do_not_optimize(value);
    }
    
    // Acquire-Release CAS
    {
        std::atomic<long long> value{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < CAS_ITERATIONS; ++i) {
            long long expected = i;
            value.compare_exchange_weak(expected, i + 1,
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "CAS (acq_rel):  " << time_ms << " ms\n";
        do_not_optimize(value);
    }
    
    // Sequential consistency CAS
    {
        std::atomic<long long> value{0};
        
        auto start = std::chrono::high_resolution_clock::now();
        for (long long i = 0; i < CAS_ITERATIONS; ++i) {
            long long expected = i;
            value.compare_exchange_weak(expected, i + 1,
                                       std::memory_order_seq_cst,
                                       std::memory_order_seq_cst);
        }
        auto end = std::chrono::high_resolution_clock::now();
        
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::cout << "CAS (seq_cst):  " << time_ms << " ms\n\n";
        do_not_optimize(value);
    }
}

/*
 * Simple spinlock implementation to demonstrate acquire-release
 */
class Spinlock {
    std::atomic<bool> locked_{false};
    
public:
    void lock() {
        // Acquire semantics: all reads/writes after lock() see updates
        // from before the previous unlock()
        while (locked_.exchange(true, std::memory_order_acquire)) {
            // Spin until we get the lock
            // Hint to CPU that we're in a spin-wait loop
            #if defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();
            #endif
        }
    }
    
    void unlock() {
        // Release semantics: all reads/writes before unlock()
        // are visible to the next thread that acquires the lock
        locked_.store(false, std::memory_order_release);
    }
};

void benchmark_spinlock() {
    std::cout << "=== Benchmark 4: Spinlock (Acquire-Release in Practice) ===\n\n";
    
    Spinlock spinlock;
    long long counter = 0;
    const long long SPIN_ITERATIONS = 1'000'000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (long long i = 0; i < SPIN_ITERATIONS; ++i) {
        spinlock.lock();
        ++counter;
        spinlock.unlock();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Single-threaded spinlock: " << std::fixed << std::setprecision(2) 
              << time_ms << " ms (" << SPIN_ITERATIONS << " lock/unlock cycles)\n";
    std::cout << "Counter value: " << counter << "\n\n";
}

int main() {
    std::cout << "=== Memory Ordering Cost Demonstration ===\n";
    std::cout << "Iterations: " << ITERATIONS << "\n\n";
    
    benchmark_counter();
    benchmark_flag_signaling();
    benchmark_cas();
    benchmark_spinlock();
    
    std::cout << "=== Memory Order Summary ===\n\n";
    
    std::cout << "memory_order_relaxed:\n";
    std::cout << "  - No synchronization, only atomicity\n";
    std::cout << "  - Fastest, but limited use cases\n";
    std::cout << "  - Good for: counters, statistics (no data dependencies)\n\n";
    
    std::cout << "memory_order_acquire (load) / memory_order_release (store):\n";
    std::cout << "  - Synchronizes data between threads\n";
    std::cout << "  - Moderate cost\n";
    std::cout << "  - Good for: producer-consumer, spinlocks, most sync patterns\n\n";
    
    std::cout << "memory_order_seq_cst:\n";
    std::cout << "  - Total ordering across all threads\n";
    std::cout << "  - Most expensive (requires full memory barriers)\n";
    std::cout << "  - Good for: when you need a total order visible to all threads\n\n";
    
    std::cout << "=== Recommendations ===\n";
    std::cout << "1. Start with seq_cst (default) for correctness\n";
    std::cout << "2. Only weaken to acq_rel or relaxed after profiling\n";
    std::cout << "3. Use relaxed for independent counters/statistics\n";
    std::cout << "4. Use acq_rel for most synchronization patterns\n";
    std::cout << "5. Keep seq_cst for complex multi-variable protocols\n";
    
    return 0;
}
