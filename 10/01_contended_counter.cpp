/*
 * 01_contended_counter.cpp
 * 
 * CONCEPT: Contended Counter Scalability
 * TECHNIQUE: Synchronization Cost / Contention
 * 
 * This program demonstrates three approaches to implementing a shared counter
 * that is incremented by multiple threads:
 *   1. std::mutex - Traditional locking (highest contention)
 *   2. std::atomic - Lock-free atomic operations (medium contention)
 *   3. Thread-local counters - No contention, combine at end (best scaling)
 * 
 * As the number of threads increases, the mutex and atomic approaches suffer
 * from contention, where threads fight over the same cache line. Thread-local
 * counters scale nearly linearly because there's no sharing during the hot loop.
 * 
 * COMPILATION:
 *   g++ -O3 -pthread -o 01_contended_counter 01_contended_counter.cpp
 * 
 * RUN:
 *   ./01_contended_counter
 */

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <numeric>

// Number of increments each thread will perform
constexpr long long INCREMENTS_PER_THREAD = 10'000'000;

/*
 * Approach 1: Mutex-protected counter
 * 
 * Every increment requires:
 *   1. Acquiring the mutex (potential sleep if contended)
 *   2. Incrementing the counter
 *   3. Releasing the mutex
 * 
 * Under high contention, threads spend most time waiting for the lock.
 * This is the SLOWEST approach for highly contended updates.
 */
struct MutexCounter {
    std::mutex mtx;
    long long value = 0;
    
    void increment() {
        std::lock_guard<std::mutex> lock(mtx);
        ++value;
    }
    
    long long get() const { return value; }
};

/*
 * Approach 2: Atomic counter
 * 
 * Uses hardware atomic instructions (e.g., LOCK INC on x86).
 * No mutex required, but the cache line containing the counter
 * ping-pongs between cores (cache line bouncing).
 * 
 * Better than mutex but still has contention overhead.
 */
struct AtomicCounter {
    std::atomic<long long> value{0};
    
    void increment() {
        // fetch_add is an atomic read-modify-write operation
        value.fetch_add(1, std::memory_order_relaxed);
    }
    
    long long get() const { return value.load(); }
};

/*
 * Approach 3: Thread-local counters
 * 
 * Each thread maintains its own counter - NO CONTENTION during updates.
 * At the end, all thread-local values are summed (one-time cost).
 * 
 * This is the FASTEST approach and scales linearly with cores.
 */
struct ThreadLocalCounter {
    // Vector to hold per-thread counters
    // Each thread gets its own slot, indexed by thread number
    std::vector<long long> thread_counters;
    
    ThreadLocalCounter(int num_threads) : thread_counters(num_threads, 0) {}
    
    void increment(int thread_id) {
        // Each thread only touches its own counter - no contention!
        ++thread_counters[thread_id];
    }
    
    long long get() const {
        // Sum all thread-local values (done once at the end)
        return std::accumulate(thread_counters.begin(), thread_counters.end(), 0LL);
    }
};

/*
 * Benchmark helper
 */
template<typename Func>
double run_benchmark(Func f) {
    auto start = std::chrono::high_resolution_clock::now();
    f();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

/*
 * Worker function for mutex counter
 */
void mutex_worker(MutexCounter& counter, long long iterations) {
    for (long long i = 0; i < iterations; ++i) {
        counter.increment();
    }
}

/*
 * Worker function for atomic counter
 */
void atomic_worker(AtomicCounter& counter, long long iterations) {
    for (long long i = 0; i < iterations; ++i) {
        counter.increment();
    }
}

/*
 * Worker function for thread-local counter
 */
void threadlocal_worker(ThreadLocalCounter& counter, int thread_id, long long iterations) {
    for (long long i = 0; i < iterations; ++i) {
        counter.increment(thread_id);
    }
}

int main() {
    std::cout << "=== Contended Counter Scalability Test ===\n";
    std::cout << "Increments per thread: " << INCREMENTS_PER_THREAD << "\n";
    std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << "\n\n";
    
    // Test with varying thread counts
    std::vector<int> thread_counts = {1, 2, 4};
    
    // Add more if available
    unsigned int hw_threads = std::thread::hardware_concurrency();
    if (hw_threads >= 8) thread_counts.push_back(8);
    if (hw_threads >= 16) thread_counts.push_back(16);
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "| Threads |  Mutex (ms)  |  Atomic (ms) | ThreadLocal (ms) | Mutex/TL Ratio |\n";
    std::cout << "|---------|--------------|--------------|------------------|----------------|\n";
    
    for (int num_threads : thread_counts) {
        // ========== Mutex Counter ==========
        MutexCounter mutex_counter;
        double mutex_time = run_benchmark([&]() {
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back(mutex_worker, std::ref(mutex_counter), 
                                    INCREMENTS_PER_THREAD / num_threads);
            }
            for (auto& t : threads) t.join();
        });
        
        // ========== Atomic Counter ==========
        AtomicCounter atomic_counter;
        double atomic_time = run_benchmark([&]() {
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back(atomic_worker, std::ref(atomic_counter),
                                    INCREMENTS_PER_THREAD / num_threads);
            }
            for (auto& t : threads) t.join();
        });
        
        // ========== Thread-Local Counter ==========
        ThreadLocalCounter tl_counter(num_threads);
        double tl_time = run_benchmark([&]() {
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back(threadlocal_worker, std::ref(tl_counter), i,
                                    INCREMENTS_PER_THREAD / num_threads);
            }
            for (auto& t : threads) t.join();
        });
        
        // Verify correctness
        long long expected = INCREMENTS_PER_THREAD - (INCREMENTS_PER_THREAD % num_threads);
        bool correct = (mutex_counter.get() == expected) && 
                       (atomic_counter.get() == expected) &&
                       (tl_counter.get() == expected);
        
        std::cout << "|    " << std::setw(4) << num_threads << " | " 
                  << std::setw(12) << mutex_time << " | "
                  << std::setw(12) << atomic_time << " | "
                  << std::setw(16) << tl_time << " | "
                  << std::setw(14) << (mutex_time / tl_time) << " |"
                  << (correct ? "" : " MISMATCH!") << "\n";
    }
    
    std::cout << "\n=== Analysis ===\n";
    std::cout << "1. MUTEX: Slowest due to kernel-level synchronization overhead.\n";
    std::cout << "   - Threads may sleep/wake, causing context switches.\n";
    std::cout << "   - Only one thread can increment at a time.\n\n";
    
    std::cout << "2. ATOMIC: Faster than mutex, but still has contention.\n";
    std::cout << "   - Uses hardware atomic instructions (no kernel calls).\n";
    std::cout << "   - Cache line bounces between cores (MESI protocol).\n";
    std::cout << "   - Performance degrades with more threads.\n\n";
    
    std::cout << "3. THREAD-LOCAL: Best scaling, no contention during updates.\n";
    std::cout << "   - Each thread works on its own cache line.\n";
    std::cout << "   - Only one synchronization point at the end.\n";
    std::cout << "   - Linear scaling with number of cores.\n\n";
    
    std::cout << "=== Recommendation ===\n";
    std::cout << "Use thread-local accumulation for counters and reductions.\n";
    std::cout << "Combine results only at synchronization points (e.g., end of parallel region).\n";
    
    return 0;
}
