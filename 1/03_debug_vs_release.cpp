/**
 * Debug vs. Release Test
 * 
 * This code demonstrates the dramatic performance difference between
 * Debug (-O0) and Release (-O3) compilation modes.
 * 
 * Key concepts:
 * - Debug mode (-O0): No optimization, easy debugging, slow execution
 * - Release mode (-O3): Aggressive optimization, fast execution
 * - Compiler optimizations include: inlining, loop unrolling, vectorization,
 *   dead code elimination, constant propagation, and more
 * 
 * To see the difference, compile this file twice:
 * 
 * Debug build:
 *   g++ -O0 -std=c++17 -o 03_debug 03_debug_vs_release.cpp
 * 
 * Release build:
 *   g++ -O3 -std=c++17 -o 03_release 03_debug_vs_release.cpp
 * 
 * Then run both:
 *   ./03_debug
 *   ./03_release
 * 
 * Compare the execution times - Release can be 10x-100x faster!
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
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

// Prevent dead code elimination
volatile double sink;

// Test 1: Simple numerical function
// In debug mode, each operation is a separate instruction
// In release mode, the compiler may vectorize and unroll this loop
double sum_of_squares(const std::vector<double>& data) {
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        // In debug: Each line is a separate operation
        // In release: The compiler may combine these, use SIMD, etc.
        double val = data[i];
        double squared = val * val;
        sum += squared;
    }
    return sum;
}

// Test 2: Function with potential for inlining
// Small functions benefit greatly from inlining in release mode
inline double square(double x) {
    return x * x;
}

inline double cube(double x) {
    return x * x * x;
}

// This function calls small functions in a loop
// In debug: Each call has overhead (push args, jump, return)
// In release: Functions are inlined, eliminating call overhead
double polynomial_sum(const std::vector<double>& data) {
    double sum = 0.0;
    for (const auto& x : data) {
        // In debug: 3 function calls per iteration
        // In release: All inlined into a single expression
        sum += square(x) + cube(x) + std::sqrt(std::abs(x));
    }
    return sum;
}

// Test 3: Loop with invariant computation
// Release mode will hoist invariants out of the loop
double loop_with_invariant(const std::vector<double>& data, double factor) {
    double sum = 0.0;
    for (size_t i = 0; i < data.size(); ++i) {
        // This computation is invariant (same every iteration)
        // Debug: Computed every iteration
        // Release: Computed once, hoisted out of loop
        double multiplier = std::sin(factor) * std::cos(factor) + 1.0;
        sum += data[i] * multiplier;
    }
    return sum;
}

// Test 4: Nested loops - shows dramatic difference
// Release mode can optimize nested loops significantly
double matrix_operation(int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        for (int j = 0; j < size; ++j) {
            // Simple computation in nested loop
            // Release mode may vectorize inner loop
            sum += static_cast<double>(i * j) / (i + j + 1);
        }
    }
    return sum;
}

// Test 5: Branch-heavy code
// Release mode can optimize predictable branches
double conditional_sum(const std::vector<double>& data) {
    double sum_positive = 0.0;
    double sum_negative = 0.0;
    
    for (const auto& x : data) {
        // Branch - compiler may generate branchless code in release
        if (x >= 0) {
            sum_positive += x;
        } else {
            sum_negative += x;
        }
    }
    
    return sum_positive - sum_negative;
}

int main() {
    std::cout << "=== Debug vs. Release Performance Test ===\n\n";
    
    // Detect optimization level (approximate)
    // This is a rough heuristic based on timing
    #ifdef __OPTIMIZE__
        std::cout << "Compiled with optimizations ENABLED (likely -O2 or -O3)\n";
    #else
        std::cout << "Compiled with optimizations DISABLED (likely -O0)\n";
    #endif
    
    std::cout << "\nCompiler: " << __VERSION__ << "\n\n";
    
    // Prepare test data
    const size_t N = 5000000;  // 5 million elements
    std::vector<double> data(N);
    
    // Initialize with values from -1000 to 1000
    for (size_t i = 0; i < N; ++i) {
        data[i] = (static_cast<double>(i) / N) * 2000.0 - 1000.0;
    }
    
    std::cout << "Test data size: " << N << " elements\n";
    std::cout << "Running each test 5 times and averaging...\n\n";
    
    const int num_runs = 5;
    
    // Test 1: Sum of Squares
    std::cout << "--- Test 1: Sum of Squares ---\n";
    {
        double total_time = 0;
        double result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = sum_of_squares(data);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << std::scientific << result << "\n\n";
    }
    
    // Test 2: Polynomial Sum (tests inlining)
    std::cout << "--- Test 2: Polynomial Sum (inlining test) ---\n";
    {
        double total_time = 0;
        double result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = polynomial_sum(data);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << std::scientific << result << "\n\n";
    }
    
    // Test 3: Loop Invariant Hoisting
    std::cout << "--- Test 3: Loop Invariant Hoisting ---\n";
    {
        double total_time = 0;
        double result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = loop_with_invariant(data, 3.14159);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << std::scientific << result << "\n\n";
    }
    
    // Test 4: Nested Loops
    std::cout << "--- Test 4: Nested Loops (matrix-like) ---\n";
    {
        double total_time = 0;
        double result;
        const int matrix_size = 2000;  // 2000x2000 iterations
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = matrix_operation(matrix_size);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << std::scientific << result << "\n\n";
    }
    
    // Test 5: Conditional/Branch Heavy Code
    std::cout << "--- Test 5: Conditional Sum (branch test) ---\n";
    {
        double total_time = 0;
        double result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = conditional_sum(data);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << std::scientific << result << "\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n";
    std::cout << "To see the optimization difference:\n\n";
    std::cout << "1. Compile with NO optimization (Debug):\n";
    std::cout << "   g++ -O0 -std=c++17 -o debug 03_debug_vs_release.cpp\n\n";
    std::cout << "2. Compile with FULL optimization (Release):\n";
    std::cout << "   g++ -O3 -std=c++17 -o release 03_debug_vs_release.cpp\n\n";
    std::cout << "3. Run both and compare times:\n";
    std::cout << "   ./debug\n";
    std::cout << "   ./release\n\n";
    std::cout << "Expected: Release version should be 5x-50x faster!\n\n";
    
    std::cout << "Key compiler optimizations in -O3:\n";
    std::cout << "- Function inlining (eliminates call overhead)\n";
    std::cout << "- Loop unrolling (reduces loop overhead)\n";
    std::cout << "- Vectorization/SIMD (processes multiple elements at once)\n";
    std::cout << "- Constant propagation (computes constants at compile time)\n";
    std::cout << "- Loop invariant hoisting (moves invariants out of loops)\n";
    std::cout << "- Dead code elimination (removes unused code)\n";
    std::cout << "- Register allocation (keeps values in fast registers)\n";
    
    return 0;
}
