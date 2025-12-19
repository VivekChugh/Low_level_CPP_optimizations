/*
 * 01_dead_code_elimination.cpp
 * 
 * CONCEPT: Preventing Dead-Code Elimination
 * TECHNIQUE: Micro-Benchmark Limitations
 * 
 * This program demonstrates a critical pitfall in benchmarking: Dead Code
 * Elimination (DCE). Modern optimizing compilers are extremely aggressive
 * at removing code that has no observable effect.
 * 
 * If you compute a value but never use it (print, store to volatile, return),
 * the compiler may eliminate the entire computation, leading to benchmarks
 * that measure... nothing!
 * 
 * Solutions demonstrated:
 *   1. Volatile sink - simple but can affect optimizations
 *   2. DoNotOptimize pattern - inline assembly barrier
 *   3. Return/Use the value - most natural approach
 * 
 * COMPILATION:
 *   # With optimization (shows the problem):
 *   g++ -O3 -o 01_dead_code_elimination 01_dead_code_elimination.cpp
 * 
 *   # To see what the compiler generates:
 *   g++ -O3 -S -fverbose-asm 01_dead_code_elimination.cpp
 * 
 * RUN:
 *   ./01_dead_code_elimination
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <iomanip>
#include <cmath>

/*
 * DoNotOptimize: Prevent the compiler from optimizing away a value
 * 
 * This inline assembly tells the compiler that:
 *   1. The value may be read/written ("+r")
 *   2. Memory may be modified ("memory" clobber)
 * 
 * This prevents DCE without actually doing anything at runtime.
 * This is the technique used by Google Benchmark.
 */
template<typename T>
void DoNotOptimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

// For const references (read-only)
template<typename T>
void DoNotOptimize(const T& value) {
    asm volatile("" : : "r"(value) : "memory");
}

/*
 * Expensive computation that the compiler might eliminate
 */
double expensive_calculation(int iterations) {
    double result = 1.0;
    for (int i = 1; i <= iterations; ++i) {
        result += std::sin(i) * std::cos(i) / (i + 1.0);
    }
    return result;
}

/*
 * Simple sum that's very easy to optimize away
 */
long long simple_sum(long long n) {
    long long sum = 0;
    for (long long i = 1; i <= n; ++i) {
        sum += i;
    }
    return sum;
}

/*
 * Benchmark helper
 */
template<typename Func>
double benchmark(const char* name, Func f, int runs = 5) {
    // Warm-up
    for (int i = 0; i < 3; ++i) f();
    
    double total_time = 0;
    for (int run = 0; run < runs; ++run) {
        auto start = std::chrono::high_resolution_clock::now();
        f();
        auto end = std::chrono::high_resolution_clock::now();
        total_time += std::chrono::duration<double, std::milli>(end - start).count();
    }
    return total_time / runs;
}

int main() {
    std::cout << "=== Dead Code Elimination Demonstration ===\n\n";
    
    const int ITERATIONS = 10'000'000;
    const long long SUM_N = 100'000'000;
    
    std::cout << std::fixed << std::setprecision(3);
    
    // ========== Test 1: Result not used (may be eliminated) ==========
    std::cout << "--- Test 1: Expensive Calculation ---\n\n";
    
    // BAD: Result not used - compiler may eliminate the entire loop!
    double time_eliminated = benchmark("Not used", [&]() {
        double result = expensive_calculation(ITERATIONS);
        // Result is never used - compiler might skip the computation!
        (void)result;  // Cast to void doesn't help
    });
    std::cout << "Result NOT used:     " << time_eliminated << " ms";
    std::cout << "  <-- May be near-zero if optimized away!\n";
    
    // GOOD: Using DoNotOptimize to prevent elimination
    double time_dno = benchmark("DoNotOptimize", [&]() {
        double result = expensive_calculation(ITERATIONS);
        DoNotOptimize(result);  // Tells compiler: "this value matters!"
    });
    std::cout << "With DoNotOptimize:  " << time_dno << " ms\n";
    
    // GOOD: Using volatile to force the write
    volatile double sink;
    double time_volatile = benchmark("Volatile sink", [&]() {
        double result = expensive_calculation(ITERATIONS);
        sink = result;  // Writing to volatile prevents elimination
    });
    std::cout << "Volatile sink:       " << time_volatile << " ms\n\n";
    
    // ========== Test 2: Simple sum (very easy to optimize) ==========
    std::cout << "--- Test 2: Simple Sum (n=" << SUM_N << ") ---\n\n";
    std::cout << "The compiler knows sum(1..n) = n*(n+1)/2 and may use that!\n\n";
    
    // BAD: Result not used
    double time_sum_eliminated = benchmark("Sum not used", [&]() {
        long long sum = simple_sum(SUM_N);
        (void)sum;
    });
    std::cout << "Result NOT used:     " << time_sum_eliminated << " ms\n";
    
    // Check with DoNotOptimize
    double time_sum_dno = benchmark("Sum DoNotOptimize", [&]() {
        long long sum = simple_sum(SUM_N);
        DoNotOptimize(sum);
    });
    std::cout << "With DoNotOptimize:  " << time_sum_dno << " ms\n";
    
    // Actually use and print the result
    long long actual_sum = 0;
    double time_sum_used = benchmark("Sum actually used", [&]() {
        actual_sum = simple_sum(SUM_N);
    });
    std::cout << "Result used (stored):" << time_sum_used << " ms";
    std::cout << " (sum = " << actual_sum << ")\n\n";
    
    // ========== Test 3: Loop invariant hoisting + DCE ==========
    std::cout << "--- Test 3: Multiple Operations ---\n\n";
    
    std::vector<double> data(1'000'000, 1.0);
    
    // BAD: Sum not used
    double time_vec_bad = benchmark("Vector sum unused", [&]() {
        double sum = std::accumulate(data.begin(), data.end(), 0.0);
        (void)sum;
    });
    std::cout << "Sum NOT used:        " << time_vec_bad << " ms\n";
    
    // GOOD: Sum protected
    double vec_sum = 0;
    double time_vec_good = benchmark("Vector sum protected", [&]() {
        vec_sum = std::accumulate(data.begin(), data.end(), 0.0);
        DoNotOptimize(vec_sum);
    });
    std::cout << "With DoNotOptimize:  " << time_vec_good << " ms (sum = " << vec_sum << ")\n\n";
    
    // ========== Summary ==========
    std::cout << "=== Key Takeaways ===\n\n";
    
    std::cout << "1. PROBLEM: Compilers aggressively remove 'useless' code\n";
    std::cout << "   - If a value isn't used, computation may be skipped\n";
    std::cout << "   - Loop may be replaced with closed-form formula\n";
    std::cout << "   - Entire functions may be eliminated\n\n";
    
    std::cout << "2. SYMPTOMS of DCE in benchmarks:\n";
    std::cout << "   - Unrealistically fast results (near-zero time)\n";
    std::cout << "   - Times don't scale with problem size\n";
    std::cout << "   - Release much faster than expected vs Debug\n\n";
    
    std::cout << "3. SOLUTIONS:\n";
    std::cout << "   a) DoNotOptimize(result) - best, minimal overhead\n";
    std::cout << "   b) volatile sink = result - simple but may affect opts\n";
    std::cout << "   c) Return and use the value - most natural\n";
    std::cout << "   d) Print the result after timing - easiest\n\n";
    
    std::cout << "4. TOOLS:\n";
    std::cout << "   - Google Benchmark: has built-in DoNotOptimize\n";
    std::cout << "   - Compiler Explorer: see if code is actually generated\n";
    std::cout << "   - objdump -d: inspect generated assembly\n";
    
    return 0;
}
