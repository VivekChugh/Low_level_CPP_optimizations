/**
 * Inlining and Call Overhead
 * 
 * This code demonstrates the benefits of function inlining - a key compiler optimization.
 * It shows how function call overhead (stack manipulation, jump instructions) is
 * eliminated when the compiler inlines small functions.
 * 
 * Key concepts:
 * - Function call overhead: push arguments, save registers, jump, return
 * - Inlining: Compiler replaces function call with the function body
 * - At -O0: Every call has overhead (~10-20 cycles per call)
 * - At -O3: Small functions are inlined, overhead eliminated
 * 
 * To see the difference:
 * 1. Compile with -O0 (no inlining) and -O3 (aggressive inlining)
 * 2. Compare execution times
 * 3. Optionally inspect assembly using: g++ -S -O3 ... or Compiler Explorer
 * 
 * Compile Debug:   g++ -O0 -std=c++17 -o 01_inline_debug 01_inlining_call_overhead.cpp
 * Compile Release: g++ -O3 -std=c++17 -o 01_inline_release 01_inlining_call_overhead.cpp
 * 
 * Run both and compare:
 *   ./01_inline_debug
 *   ./01_inline_release
 */

#include <iostream>
#include <chrono>
#include <iomanip>
#include <cmath>

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
 * A very small helper function
 * 
 * At -O0 (Debug):
 * - Each call involves: push arguments to stack, jump to function,
 *   execute body, pop return value, return to caller
 * - This overhead can be 10-20+ cycles per call
 * 
 * At -O3 (Release):
 * - The compiler sees this function is tiny
 * - It replaces the call with just: return a + b;
 * - No stack manipulation, no jump, no return overhead
 */
inline int add(int a, int b) {
    return a + b;
}

/**
 * Another small helper - multiplication
 */
inline int multiply(int a, int b) {
    return a * b;
}

/**
 * Small helper - increment
 */
inline int increment(int x) {
    return x + 1;
}

/**
 * Compound operation using multiple small functions
 * At -O3, all these calls should be inlined into one expression
 */
inline int compound_operation(int a, int b, int c) {
    // At -O0: 3 function calls with full overhead
    // At -O3: Becomes roughly: return (a + b) * c + 1;
    int sum = add(a, b);
    int product = multiply(sum, c);
    return increment(product);
}

/**
 * Test 1: Tight loop calling a tiny function
 * 
 * This is where inlining shines most dramatically.
 * Without inlining, millions of function calls create massive overhead.
 * With inlining, it becomes a simple loop with arithmetic.
 */
long long test_tiny_function_calls(long long iterations) {
    long long result = 0;
    
    for (long long i = 0; i < iterations; ++i) {
        // Each iteration calls add()
        // At -O0: Function call overhead every iteration
        // At -O3: Becomes: result += i + 1;
        result = add(result, static_cast<int>(i % 1000));
    }
    
    return result;
}

/**
 * Test 2: Multiple function calls per iteration
 * 
 * Even more dramatic difference when multiple small functions
 * are called per iteration.
 */
long long test_multiple_calls(long long iterations) {
    long long result = 0;
    
    for (long long i = 0; i < iterations; ++i) {
        // Multiple function calls per iteration
        // At -O0: 4+ function calls with overhead
        // At -O3: All inlined into simple arithmetic
        int a = static_cast<int>(i % 100);
        int b = static_cast<int>((i + 1) % 100);
        int c = static_cast<int>((i + 2) % 100);
        
        result += compound_operation(a, b, c);
    }
    
    return result;
}

/**
 * Test 3: Direct arithmetic (baseline)
 * 
 * This represents what the code looks like AFTER inlining.
 * At -O3, test_tiny_function_calls should be nearly as fast as this.
 */
long long test_direct_arithmetic(long long iterations) {
    long long result = 0;
    
    for (long long i = 0; i < iterations; ++i) {
        // Direct arithmetic - no function calls
        // This is what inlined code essentially becomes
        result += (i % 1000);
    }
    
    return result;
}

/**
 * A function marked with __attribute__((noinline)) to prevent inlining
 * This lets us see the "forced overhead" even at -O3
 */
#ifdef __GNUC__
__attribute__((noinline))
#endif
int add_noinline(int a, int b) {
    return a + b;
}

/**
 * Test 4: Forced non-inlined calls
 * 
 * Even at -O3, this function won't be inlined due to the attribute.
 * This shows what performance would be like without inlining.
 */
long long test_noinline_calls(long long iterations) {
    long long result = 0;
    
    for (long long i = 0; i < iterations; ++i) {
        // This call is NOT inlined even at -O3
        result = add_noinline(static_cast<int>(result % 1000000), 
                              static_cast<int>(i % 1000));
    }
    
    return result;
}

int main() {
    std::cout << "=== Inlining and Call Overhead Demo ===\n\n";
    
    // Detect optimization level (heuristic)
    #ifdef __OPTIMIZE__
        std::cout << "Compiled with OPTIMIZATIONS ENABLED (likely -O2 or -O3)\n";
        std::cout << "Small functions should be INLINED\n\n";
    #else
        std::cout << "Compiled with OPTIMIZATIONS DISABLED (likely -O0)\n";
        std::cout << "Functions are NOT inlined - expect slower performance\n\n";
    #endif
    
    const long long ITERATIONS = 100000000;  // 100 million
    const int num_runs = 5;
    
    std::cout << "Iterations: " << ITERATIONS << "\n";
    std::cout << "Runs per test: " << num_runs << "\n\n";
    
    // Warm-up
    sink = test_tiny_function_calls(ITERATIONS / 100);
    sink = test_direct_arithmetic(ITERATIONS / 100);
    
    // Test 1: Tiny function calls
    std::cout << "--- Test 1: Tiny Function Calls (add) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_tiny_function_calls(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 2: Multiple calls per iteration
    std::cout << "--- Test 2: Multiple Function Calls per Iteration ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_multiple_calls(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 3: Direct arithmetic (baseline)
    std::cout << "--- Test 3: Direct Arithmetic (no function calls) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_direct_arithmetic(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "(This is the baseline - what inlined code should approach)\n\n";
    }
    
    // Test 4: Forced non-inlined calls
    std::cout << "--- Test 4: Forced Non-Inlined Calls ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_noinline_calls(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "(This shows call overhead even at -O3)\n\n";
    }
    
    // Summary and instructions
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "To see the full effect of inlining:\n\n";
    
    std::cout << "1. Compile WITHOUT optimization (Debug):\n";
    std::cout << "   g++ -O0 -std=c++17 -o debug 01_inlining_call_overhead.cpp\n\n";
    
    std::cout << "2. Compile WITH optimization (Release):\n";
    std::cout << "   g++ -O3 -std=c++17 -o release 01_inlining_call_overhead.cpp\n\n";
    
    std::cout << "3. Run both and compare:\n";
    std::cout << "   ./debug\n";
    std::cout << "   ./release\n\n";
    
    std::cout << "Expected observations:\n";
    std::cout << "- Test 1 & 2: MUCH faster at -O3 (functions inlined)\n";
    std::cout << "- Test 3: Similar speed at -O0 and -O3 (no calls)\n";
    std::cout << "- Test 4: Shows overhead even at -O3 (forced no-inline)\n\n";
    
    std::cout << "To inspect assembly:\n";
    std::cout << "   g++ -S -O3 -std=c++17 01_inlining_call_overhead.cpp -o output.s\n";
    std::cout << "   Or use https://godbolt.org (Compiler Explorer)\n\n";
    
    std::cout << "What inlining eliminates:\n";
    std::cout << "- Stack frame setup/teardown\n";
    std::cout << "- Argument passing overhead\n";
    std::cout << "- Jump/call instructions\n";
    std::cout << "- Return value handling\n";
    std::cout << "- Enables further optimizations (constant folding, etc.)\n";
    
    return 0;
}
