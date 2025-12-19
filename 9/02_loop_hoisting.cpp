/*
 * 02_loop_hoisting.cpp
 * 
 * CONCEPT: Loop Transformations (Hoist Invariants)
 * TECHNIQUE: Helping the Compiler
 * 
 * This program demonstrates the importance of hoisting loop-invariant
 * computations outside of loops. While modern compilers can often do this
 * automatically (loop-invariant code motion - LICM), helping the compiler
 * by manually hoisting invariants can:
 *   1. Make code intent clearer
 *   2. Enable further optimizations like vectorization
 *   3. Improve instruction-level parallelism (ILP)
 * 
 * A loop-invariant is any value that doesn't change during loop iterations.
 * Computing it repeatedly wastes CPU cycles and may prevent vectorization.
 * 
 * COMPILATION:
 *   # Debug (no optimization):
 *   g++ -O0 -o 02_loop_hoisting_debug 02_loop_hoisting.cpp
 * 
 *   # Release (with optimization):
 *   g++ -O3 -march=native -o 02_loop_hoisting 02_loop_hoisting.cpp
 * 
 *   # With vectorization report:
 *   g++ -O3 -march=native -fopt-info-vec -o 02_loop_hoisting 02_loop_hoisting.cpp
 * 
 * RUN:
 *   ./02_loop_hoisting
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>

// Prevent dead code elimination
template<typename T>
void do_not_optimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

/*
 * BAD: Loop-invariant computation inside the loop
 * 
 * The expression (std::sqrt(scale) * multiplier) is computed
 * every iteration even though its value never changes.
 * This wastes CPU cycles and may prevent vectorization.
 */
void process_bad(float* data, size_t size, float scale, float multiplier) {
    for (size_t i = 0; i < size; ++i) {
        // sqrt(scale) * multiplier is computed SIZE times!
        data[i] = data[i] * std::sqrt(scale) * multiplier;
    }
}

/*
 * GOOD: Loop-invariant hoisted outside the loop
 * 
 * Pre-computing the invariant factor:
 *   1. Reduces redundant computation
 *   2. Makes vectorization easier for the compiler
 *   3. Results in identical output
 */
void process_good(float* data, size_t size, float scale, float multiplier) {
    // Hoist invariant outside the loop
    const float factor = std::sqrt(scale) * multiplier;
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = data[i] * factor;  // Simple multiply, easily vectorized
    }
}

/*
 * WORSE: Multiple invariants and function calls inside loop
 * 
 * This demonstrates a more complex case where multiple
 * expensive operations are performed redundantly.
 */
void process_worse(float* data, size_t size, float a, float b, float c) {
    for (size_t i = 0; i < size; ++i) {
        // Multiple invariants computed every iteration
        float base = std::pow(a, b);
        float offset = std::sin(c) * std::cos(c);
        data[i] = data[i] * base + offset;
    }
}

/*
 * BETTER: All invariants hoisted
 * 
 * Moving all invariant computations outside the loop
 * dramatically improves performance.
 */
void process_better(float* data, size_t size, float a, float b, float c) {
    // Hoist all invariants
    const float base = std::pow(a, b);
    const float offset = std::sin(c) * std::cos(c);
    
    for (size_t i = 0; i < size; ++i) {
        data[i] = data[i] * base + offset;  // Simple, vectorizable
    }
}

/*
 * Benchmark helper
 */
template<typename Func>
double benchmark(Func f, int iterations) {
    // Warm-up
    for (int i = 0; i < 5; ++i) f();
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        f();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

int main() {
    const size_t SIZE = 5'000'000;  // 5 million elements
    const int ITERATIONS = 50;
    
    std::cout << "=== Loop Hoisting Optimization Demonstration ===\n";
    std::cout << "Array size: " << SIZE << " elements\n\n";
    
    // Test data
    std::vector<float> data1(SIZE, 1.0f);
    std::vector<float> data2(SIZE, 1.0f);
    std::vector<float> data3(SIZE, 1.0f);
    std::vector<float> data4(SIZE, 1.0f);
    
    // Parameters for the functions
    float scale = 4.0f;
    float multiplier = 2.5f;
    float a = 2.0f, b = 3.0f, c = 1.5f;
    
    // ========== Test 1: Simple invariant ==========
    std::cout << "--- Test 1: Simple sqrt() Invariant ---\n";
    
    double bad_time = benchmark([&]() {
        process_bad(data1.data(), SIZE, scale, multiplier);
        do_not_optimize(data1[0]);
    }, ITERATIONS);
    
    double good_time = benchmark([&]() {
        process_good(data2.data(), SIZE, scale, multiplier);
        do_not_optimize(data2[0]);
    }, ITERATIONS);
    
    std::cout << "Without hoisting: " << std::fixed << std::setprecision(3) << bad_time << " ms\n";
    std::cout << "With hoisting:    " << good_time << " ms\n";
    std::cout << "Speedup:          " << bad_time / good_time << "x\n\n";
    
    // ========== Test 2: Complex invariants ==========
    std::cout << "--- Test 2: Complex pow()/sin()/cos() Invariants ---\n";
    
    double worse_time = benchmark([&]() {
        process_worse(data3.data(), SIZE, a, b, c);
        do_not_optimize(data3[0]);
    }, ITERATIONS);
    
    double better_time = benchmark([&]() {
        process_better(data4.data(), SIZE, a, b, c);
        do_not_optimize(data4[0]);
    }, ITERATIONS);
    
    std::cout << "Without hoisting: " << worse_time << " ms\n";
    std::cout << "With hoisting:    " << better_time << " ms\n";
    std::cout << "Speedup:          " << worse_time / better_time << "x\n\n";
    
    // ========== Summary ==========
    std::cout << "=== Key Takeaways ===\n";
    std::cout << "1. Loop-invariant code motion (LICM) is a critical optimization\n";
    std::cout << "2. Compilers do LICM automatically at -O2/-O3, but not always\n";
    std::cout << "3. Explicitly hoisting makes code clearer and more reliable\n";
    std::cout << "4. Function calls (sqrt, pow, sin) are especially expensive to repeat\n";
    std::cout << "5. Hoisting enables better vectorization of the remaining loop\n";
    
    return 0;
}
