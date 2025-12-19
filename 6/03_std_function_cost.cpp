/**
 * std::function Overhead Analysis
 * 
 * This code analyzes the specific overhead components of std::function:
 * 1. Type erasure mechanism
 * 2. Small buffer optimization (SBO) vs heap allocation
 * 3. Indirect function calls
 * 4. Copy/move overhead
 * 
 * Key concepts:
 * - std::function uses type erasure to store any callable
 * - Small callables fit in internal buffer (SBO)
 * - Large callables require heap allocation
 * - Every call goes through an indirect pointer
 * 
 * Compile: g++ -O3 -std=c++17 -o 03_std_function_cost 03_std_function_cost.cpp
 * Run: ./03_std_function_cost
 */

#include <iostream>
#include <functional>
#include <chrono>
#include <iomanip>
#include <vector>
#include <array>
#include <memory>

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
    double elapsed_ns() const {
        return std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    }
};

// Prevent compiler from optimizing away results
volatile int sink;

// ============================================
// DIFFERENT CALLABLE SIZES
// ============================================

/**
 * Small lambda - fits in SBO (Small Buffer Optimization)
 * 
 * Most std::function implementations have an internal buffer
 * (typically 16-32 bytes) for small callables.
 */
auto make_small_lambda() {
    int x = 42;  // Only captures 4 bytes
    return [x](int n) { return n + x; };
}

/**
 * Medium lambda - might fit in SBO depending on implementation
 */
auto make_medium_lambda() {
    std::array<int, 4> arr = {1, 2, 3, 4};  // 16 bytes capture
    return [arr](int n) { return n + arr[0] + arr[1] + arr[2] + arr[3]; };
}

/**
 * Large lambda - likely requires heap allocation
 * 
 * When the captured state exceeds the SBO size,
 * std::function must allocate heap memory.
 */
auto make_large_lambda() {
    std::array<int, 16> arr = {};  // 64 bytes capture
    for (int i = 0; i < 16; ++i) arr[i] = i;
    return [arr](int n) { 
        int sum = n;
        for (int x : arr) sum += x;
        return sum;
    };
}

/**
 * Functor with small state
 */
struct SmallFunctor {
    int x;
    SmallFunctor(int v) : x(v) {}
    int operator()(int n) const { return n + x; }
};

/**
 * Functor with large state
 */
struct LargeFunctor {
    std::array<int, 32> data;  // 128 bytes
    LargeFunctor() {
        for (int i = 0; i < 32; ++i) data[i] = i;
    }
    int operator()(int n) const {
        int sum = n;
        for (int x : data) sum += x;
        return sum;
    }
};

// ============================================
// TEST FUNCTIONS
// ============================================

/**
 * Measure construction overhead
 */
template <typename Callable>
double measure_construction(Callable&& callable, int iterations) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        // Construct std::function from callable
        std::function<int(int)> func = callable;
        sink = func(i);
    }
    
    return timer.elapsed_ms();
}

/**
 * Measure call overhead - std::function version
 */
double measure_call_std_function(std::function<int(int)> func, int iterations) {
    Timer timer;
    int result = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // Each call goes through type erasure indirection
        result += func(i);
    }
    
    sink = result;
    return timer.elapsed_ms();
}

/**
 * Measure call overhead - direct functor version
 */
template <typename Func>
double measure_call_direct(Func func, int iterations) {
    Timer timer;
    int result = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // Direct call, can be inlined
        result += func(i);
    }
    
    sink = result;
    return timer.elapsed_ms();
}

/**
 * Measure copy overhead
 */
template <typename Callable>
double measure_copy(Callable&& callable, int iterations) {
    std::function<int(int)> original = callable;
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        // Copy the std::function
        // For large callables, this involves heap allocation
        std::function<int(int)> copy = original;
        sink = copy(i);
    }
    
    return timer.elapsed_ms();
}

/**
 * Compare passing by value vs by reference
 */
int invoke_by_value(std::function<int(int)> func, int n) {
    return func(n);
}

int invoke_by_reference(const std::function<int(int)>& func, int n) {
    return func(n);
}

double measure_pass_by_value(const std::function<int(int)>& func, int iterations) {
    Timer timer;
    int result = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // Each call copies the std::function
        result += invoke_by_value(func, i);
    }
    
    sink = result;
    return timer.elapsed_ms();
}

double measure_pass_by_reference(const std::function<int(int)>& func, int iterations) {
    Timer timer;
    int result = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // No copy, just reference
        result += invoke_by_reference(func, i);
    }
    
    sink = result;
    return timer.elapsed_ms();
}

int main() {
    std::cout << "=== std::function Overhead Analysis ===\n\n";
    
    // Configuration
    const int iterations = 10000000;  // 10 million iterations
    
    std::cout << "Iterations: " << iterations << "\n\n";
    
    // ==========================================
    // Size Analysis
    // ==========================================
    std::cout << "=== Size Analysis ===\n";
    std::cout << "sizeof(std::function<int(int)>): " 
              << sizeof(std::function<int(int)>) << " bytes\n";
    std::cout << "sizeof(SmallFunctor): " << sizeof(SmallFunctor) << " bytes\n";
    std::cout << "sizeof(LargeFunctor): " << sizeof(LargeFunctor) << " bytes\n";
    std::cout << "sizeof(void*): " << sizeof(void*) << " bytes\n";
    std::cout << "\nNote: std::function has internal buffer (SBO) for small callables.\n";
    std::cout << "Callables larger than SBO require heap allocation.\n\n";
    
    // ==========================================
    // Construction Overhead
    // ==========================================
    std::cout << "=== Construction Overhead ===\n";
    
    auto small_lambda = make_small_lambda();
    auto large_lambda = make_large_lambda();
    SmallFunctor small_functor(42);
    LargeFunctor large_functor;
    
    std::cout << "Small lambda (fits in SBO):\n";
    std::cout << "  Construction time: " << std::fixed << std::setprecision(2)
              << measure_construction(small_lambda, iterations) << " ms\n";
    
    std::cout << "Large lambda (heap allocation):\n";
    std::cout << "  Construction time: " << std::fixed << std::setprecision(2)
              << measure_construction(large_lambda, iterations) << " ms\n";
    
    std::cout << "Small functor:\n";
    std::cout << "  Construction time: " << std::fixed << std::setprecision(2)
              << measure_construction(small_functor, iterations) << " ms\n";
    
    std::cout << "Large functor:\n";
    std::cout << "  Construction time: " << std::fixed << std::setprecision(2)
              << measure_construction(large_functor, iterations) << " ms\n\n";
    
    // ==========================================
    // Call Overhead
    // ==========================================
    std::cout << "=== Call Overhead ===\n";
    
    std::function<int(int)> func_small = small_lambda;
    std::function<int(int)> func_large = large_lambda;
    
    std::cout << "Direct call (small functor):\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_call_direct(small_functor, iterations) << " ms\n";
    
    std::cout << "std::function call (small lambda):\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_call_std_function(func_small, iterations) << " ms\n";
    
    std::cout << "std::function call (large lambda):\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_call_std_function(func_large, iterations) << " ms\n\n";
    
    // ==========================================
    // Copy Overhead
    // ==========================================
    std::cout << "=== Copy Overhead ===\n";
    
    std::cout << "Copy small lambda:\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_copy(small_lambda, iterations / 10) << " ms (1M copies)\n";
    
    std::cout << "Copy large lambda:\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_copy(large_lambda, iterations / 10) << " ms (1M copies)\n\n";
    
    // ==========================================
    // Pass by Value vs Reference
    // ==========================================
    std::cout << "=== Pass by Value vs Reference ===\n";
    
    std::cout << "Pass small lambda by VALUE:\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_pass_by_value(func_small, iterations / 10) << " ms (1M calls)\n";
    
    std::cout << "Pass small lambda by REFERENCE:\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_pass_by_reference(func_small, iterations / 10) << " ms (1M calls)\n";
    
    std::cout << "Pass large lambda by VALUE:\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_pass_by_value(func_large, iterations / 10) << " ms (1M calls)\n";
    
    std::cout << "Pass large lambda by REFERENCE:\n";
    std::cout << "  Time: " << std::fixed << std::setprecision(2)
              << measure_pass_by_reference(func_large, iterations / 10) << " ms (1M calls)\n\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "std::function overhead components:\n\n";
    
    std::cout << "1. TYPE ERASURE:\n";
    std::cout << "   - Internal vtable-like mechanism\n";
    std::cout << "   - Every call is indirect (through function pointer)\n";
    std::cout << "   - Prevents inlining\n\n";
    
    std::cout << "2. SMALL BUFFER OPTIMIZATION (SBO):\n";
    std::cout << "   - Small callables: No heap allocation\n";
    std::cout << "   - Large callables: Heap allocation required\n";
    std::cout << "   - SBO size varies by implementation (~16-32 bytes)\n\n";
    
    std::cout << "3. COPY OVERHEAD:\n";
    std::cout << "   - Small callables: Just memcpy\n";
    std::cout << "   - Large callables: Heap allocation + copy\n";
    std::cout << "   - Always pass by const reference!\n\n";
    
    std::cout << "Best practices:\n\n";
    
    std::cout << "1. Prefer templates over std::function in hot paths:\n";
    std::cout << "   template <typename F>\n";
    std::cout << "   void process(F&& func) { ... }\n\n";
    
    std::cout << "2. Pass std::function by const reference:\n";
    std::cout << "   void process(const std::function<int(int)>& func);\n\n";
    
    std::cout << "3. Keep captured state small:\n";
    std::cout << "   - Capture by reference for large objects\n";
    std::cout << "   - Use shared_ptr if lifetime is complex\n\n";
    
    std::cout << "4. Use std::function only when needed:\n";
    std::cout << "   - Storing callbacks\n";
    std::cout << "   - Type-erased containers\n";
    std::cout << "   - Runtime polymorphism for callables\n";
    
    return 0;
}
