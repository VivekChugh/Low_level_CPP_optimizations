/*
 * 02_minimal_benchmark.cpp
 * 
 * CONCEPT: Minimal Benchmark Setup
 * TECHNIQUE: Measurement Best Practices
 * 
 * This program demonstrates proper benchmarking methodology including:
 *   1. Warm-up phase to stabilize CPU frequency and prime caches
 *   2. Multiple runs for statistical validity
 *   3. Outlier detection and removal
 *   4. Proper timing with high-resolution clock
 *   5. Prevention of dead code elimination
 * 
 * Without these practices, benchmarks can give misleading results due to:
 *   - CPU frequency scaling (turbo boost ramp-up)
 *   - Cold cache effects
 *   - System noise (other processes, interrupts)
 *   - Statistical variance
 * 
 * COMPILATION:
 *   g++ -O3 -o 02_minimal_benchmark 02_minimal_benchmark.cpp
 * 
 * RUN:
 *   ./02_minimal_benchmark
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <functional>

/*
 * DoNotOptimize: Prevent compiler from eliminating computation
 */
template<typename T>
void DoNotOptimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

/*
 * BenchmarkResult: Statistics from a benchmark run
 */
struct BenchmarkResult {
    double mean_ms;      // Mean time in milliseconds
    double stddev_ms;    // Standard deviation
    double min_ms;       // Minimum time
    double max_ms;       // Maximum time
    double median_ms;    // Median time
    int valid_runs;      // Number of runs after outlier removal
};

/*
 * MinimalBenchmark: A simple but proper benchmarking framework
 * 
 * Features:
 *   - Configurable warm-up iterations
 *   - Multiple measurement runs
 *   - Statistical analysis (mean, stddev, median, min, max)
 *   - Optional outlier removal
 */
class MinimalBenchmark {
public:
    MinimalBenchmark(int warmup_runs = 5, int measurement_runs = 20)
        : warmup_runs_(warmup_runs), measurement_runs_(measurement_runs) {}
    
    /*
     * Run a benchmark with proper warm-up and statistics
     * 
     * The function 'f' should:
     *   1. Perform the operation to measure
     *   2. Use DoNotOptimize on results to prevent DCE
     */
    template<typename Func>
    BenchmarkResult run(Func f) {
        // ===== Phase 1: Warm-up =====
        // Purpose: 
        //   - CPU frequency ramps up to turbo speed
        //   - Caches are primed with relevant data
        //   - Branch predictors learn the pattern
        for (int i = 0; i < warmup_runs_; ++i) {
            f();
        }
        
        // ===== Phase 2: Measurement =====
        std::vector<double> times;
        times.reserve(measurement_runs_);
        
        for (int i = 0; i < measurement_runs_; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            f();
            auto end = std::chrono::high_resolution_clock::now();
            
            double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            times.push_back(elapsed);
        }
        
        // ===== Phase 3: Statistical Analysis =====
        return analyze(times);
    }
    
private:
    int warmup_runs_;
    int measurement_runs_;
    
    /*
     * Analyze timing results and compute statistics
     */
    BenchmarkResult analyze(std::vector<double>& times) {
        BenchmarkResult result{};
        
        if (times.empty()) return result;
        
        // Sort for median and outlier detection
        std::sort(times.begin(), times.end());
        
        // Remove outliers (beyond 2 standard deviations)
        double initial_mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
        double sq_sum = 0.0;
        for (double t : times) {
            sq_sum += (t - initial_mean) * (t - initial_mean);
        }
        double initial_stddev = std::sqrt(sq_sum / times.size());
        
        // Filter outliers
        std::vector<double> filtered;
        for (double t : times) {
            if (std::abs(t - initial_mean) <= 2.0 * initial_stddev) {
                filtered.push_back(t);
            }
        }
        
        // Use filtered times if we have enough samples
        auto& final_times = (filtered.size() >= times.size() / 2) ? filtered : times;
        
        // Calculate final statistics
        result.valid_runs = static_cast<int>(final_times.size());
        result.mean_ms = std::accumulate(final_times.begin(), final_times.end(), 0.0) / final_times.size();
        
        sq_sum = 0.0;
        for (double t : final_times) {
            sq_sum += (t - result.mean_ms) * (t - result.mean_ms);
        }
        result.stddev_ms = std::sqrt(sq_sum / final_times.size());
        
        std::sort(final_times.begin(), final_times.end());
        result.min_ms = final_times.front();
        result.max_ms = final_times.back();
        result.median_ms = final_times[final_times.size() / 2];
        
        return result;
    }
};

/*
 * Pretty-print benchmark results
 */
void print_result(const char* name, const BenchmarkResult& result) {
    std::cout << std::fixed << std::setprecision(3);
    std::cout << name << ":\n";
    std::cout << "  Mean:   " << std::setw(10) << result.mean_ms << " ms (±" << result.stddev_ms << " ms)\n";
    std::cout << "  Median: " << std::setw(10) << result.median_ms << " ms\n";
    std::cout << "  Range:  [" << result.min_ms << " - " << result.max_ms << "] ms\n";
    std::cout << "  Valid runs: " << result.valid_runs << "\n\n";
}

// ==================== Test Functions ====================

/*
 * Test function 1: Vector sum
 */
double vector_sum(const std::vector<double>& v) {
    double sum = 0.0;
    for (double x : v) {
        sum += x;
    }
    return sum;
}

/*
 * Test function 2: Vector with expensive per-element operation
 */
double vector_transform(const std::vector<double>& v) {
    double sum = 0.0;
    for (double x : v) {
        sum += std::sin(x) * std::cos(x);
    }
    return sum;
}

/*
 * Test function 3: Binary search (different performance profile)
 */
int binary_search_count(const std::vector<int>& sorted_v, int target) {
    int count = 0;
    // Search for random values (simulating real usage)
    for (int i = 0; i < 10000; ++i) {
        if (std::binary_search(sorted_v.begin(), sorted_v.end(), target + i % 1000)) {
            ++count;
        }
    }
    return count;
}

int main() {
    std::cout << "=== Minimal Benchmark Framework Demonstration ===\n\n";
    
    // Setup
    const size_t SIZE = 1'000'000;
    std::vector<double> data(SIZE);
    for (size_t i = 0; i < SIZE; ++i) {
        data[i] = static_cast<double>(i) / SIZE;
    }
    
    std::vector<int> sorted_data(SIZE);
    for (size_t i = 0; i < SIZE; ++i) {
        sorted_data[i] = static_cast<int>(i);
    }
    
    // Create benchmark with custom parameters
    MinimalBenchmark bench(10, 30);  // 10 warm-up runs, 30 measurement runs
    
    std::cout << "Data size: " << SIZE << " elements\n";
    std::cout << "Warm-up: 10 runs, Measurements: 30 runs\n\n";
    
    // ========== Benchmark 1: Simple sum ==========
    std::cout << "--- Benchmark 1: Vector Sum ---\n";
    double sum_result = 0;
    auto result1 = bench.run([&]() {
        sum_result = vector_sum(data);
        DoNotOptimize(sum_result);
    });
    print_result("Vector Sum", result1);
    std::cout << "  (Sum value: " << sum_result << ")\n\n";
    
    // ========== Benchmark 2: Transform with trig functions ==========
    std::cout << "--- Benchmark 2: Vector Transform (sin/cos) ---\n";
    double transform_result = 0;
    auto result2 = bench.run([&]() {
        transform_result = vector_transform(data);
        DoNotOptimize(transform_result);
    });
    print_result("Vector Transform", result2);
    std::cout << "  (Transform value: " << transform_result << ")\n\n";
    
    // ========== Benchmark 3: Binary search ==========
    std::cout << "--- Benchmark 3: Binary Search ---\n";
    int search_result = 0;
    auto result3 = bench.run([&]() {
        search_result = binary_search_count(sorted_data, 500000);
        DoNotOptimize(search_result);
    });
    print_result("Binary Search", result3);
    std::cout << "  (Found count: " << search_result << ")\n\n";
    
    // ========== Demonstrate warm-up importance ==========
    std::cout << "--- Demonstrating Warm-up Importance ---\n\n";
    
    // No warm-up
    MinimalBenchmark no_warmup(0, 10);
    auto no_warmup_result = no_warmup.run([&]() {
        sum_result = vector_sum(data);
        DoNotOptimize(sum_result);
    });
    print_result("Without warm-up", no_warmup_result);
    
    // With warm-up
    MinimalBenchmark with_warmup(20, 10);
    auto warmup_result = with_warmup.run([&]() {
        sum_result = vector_sum(data);
        DoNotOptimize(sum_result);
    });
    print_result("With warm-up (20 iterations)", warmup_result);
    
    // ========== Summary ==========
    std::cout << "=== Best Practices Summary ===\n\n";
    
    std::cout << "1. WARM-UP PHASE:\n";
    std::cout << "   - Run the benchmark 5-20 times before measuring\n";
    std::cout << "   - Allows CPU to reach turbo frequency\n";
    std::cout << "   - Primes instruction and data caches\n";
    std::cout << "   - Trains branch predictors\n\n";
    
    std::cout << "2. MULTIPLE RUNS:\n";
    std::cout << "   - Take 20+ measurements for statistical validity\n";
    std::cout << "   - Report mean AND standard deviation\n";
    std::cout << "   - Consider median for skewed distributions\n\n";
    
    std::cout << "3. OUTLIER HANDLING:\n";
    std::cout << "   - Remove runs > 2 standard deviations from mean\n";
    std::cout << "   - System interrupts can cause random slowdowns\n";
    std::cout << "   - Report how many runs were valid\n\n";
    
    std::cout << "4. PREVENT OPTIMIZATION PITFALLS:\n";
    std::cout << "   - Use DoNotOptimize() on results\n";
    std::cout << "   - Verify assembly to ensure code runs\n";
    std::cout << "   - Check that times scale with input size\n\n";
    
    std::cout << "5. ENVIRONMENTAL FACTORS:\n";
    std::cout << "   - Close other applications\n";
    std::cout << "   - Disable CPU frequency scaling if possible\n";
    std::cout << "   - Run multiple times on different occasions\n";
    
    return 0;
}
