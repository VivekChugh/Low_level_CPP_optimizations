/**
 * Branchless Conversion - Clamp Function
 * 
 * This code demonstrates how to convert branching code to branchless code
 * using the clamp function as an example.
 * 
 * Key concepts:
 * - Conditional branches can be expensive for unpredictable data
 * - std::min/std::max often compile to branchless conditional moves (CMOV)
 * - Compiler may generate branchless code automatically with -O3
 * - Explicit branchless code guarantees no branches
 * 
 * The clamp function: clamp(x, lo, hi) returns:
 * - lo if x < lo
 * - hi if x > hi
 * - x otherwise
 * 
 * Compile: g++ -O3 -std=c++17 -o 02_branchless_clamp 02_branchless_clamp.cpp
 * Run: ./02_branchless_clamp
 * 
 * To see assembly: g++ -S -O3 -std=c++17 02_branchless_clamp.cpp -o clamp.s
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
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
volatile float sink_float;

/**
 * Version 1: Clamp with explicit if/else branches
 * 
 * This creates two branch points in the machine code.
 * Each branch can be mispredicted if x values are unpredictable.
 */
inline float clamp_branched(float x, float lo, float hi) {
    if (x < lo) {
        return lo;      // Branch 1: x is below range
    } else if (x > hi) {
        return hi;      // Branch 2: x is above range
    } else {
        return x;       // x is within range
    }
}

/**
 * Version 2: Clamp with nested ternary operators
 * 
 * Logically equivalent to if/else, but compilers often
 * generate better code for ternary operators.
 */
inline float clamp_ternary(float x, float lo, float hi) {
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * Version 3: Clamp using std::min and std::max
 * 
 * This is the idiomatic C++ way and often compiles to branchless code.
 * The compiler recognizes this pattern and generates CMOV or SIMD instructions.
 */
inline float clamp_minmax(float x, float lo, float hi) {
    // std::max(x, lo) ensures x >= lo
    // std::min(result, hi) ensures result <= hi
    return std::min(std::max(x, lo), hi);
}

/**
 * Version 4: Using std::clamp (C++17)
 * 
 * The standard library implementation, typically optimized.
 */
inline float clamp_std(float x, float lo, float hi) {
    return std::clamp(x, lo, hi);
}

/**
 * Version 5: Explicit branchless using bit manipulation (for integers)
 * 
 * This demonstrates the concept for integer types.
 * For floats, we'd need to handle the bit representation carefully.
 */
inline int clamp_branchless_int(int x, int lo, int hi) {
    // Use arithmetic to avoid branches
    // (x - lo) >> 31 gives -1 if x < lo, 0 otherwise (for 32-bit int)
    // We use this to select between lo and x
    
    int below = (x - lo) >> 31;  // -1 if x < lo, 0 otherwise
    int above = (hi - x) >> 31;  // -1 if x > hi, 0 otherwise
    
    // If below is -1, result = lo; else result = x
    int clamped_low = (lo & below) | (x & ~below);
    
    // If above is -1, result = hi; else result = clamped_low
    return (hi & above) | (clamped_low & ~above);
}

/**
 * Apply clamp to entire array - branched version
 */
void apply_clamp_branched(std::vector<float>& data, float lo, float hi) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = clamp_branched(data[i], lo, hi);
    }
}

/**
 * Apply clamp to entire array - ternary version
 */
void apply_clamp_ternary(std::vector<float>& data, float lo, float hi) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = clamp_ternary(data[i], lo, hi);
    }
}

/**
 * Apply clamp to entire array - min/max version
 */
void apply_clamp_minmax(std::vector<float>& data, float lo, float hi) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = clamp_minmax(data[i], lo, hi);
    }
}

/**
 * Apply clamp to entire array - std::clamp version
 */
void apply_clamp_std(std::vector<float>& data, float lo, float hi) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = clamp_std(data[i], lo, hi);
    }
}

/**
 * Apply clamp using std::transform with std::clamp
 */
void apply_clamp_transform(std::vector<float>& data, float lo, float hi) {
    std::transform(data.begin(), data.end(), data.begin(),
        [lo, hi](float x) { return std::clamp(x, lo, hi); });
}

int main() {
    std::cout << "=== Branchless Clamp Demo ===\n\n";
    
    // Configuration
    const size_t N = 50000000;  // 50 million elements
    const float LO = 0.25f;
    const float HI = 0.75f;
    const int num_runs = 5;
    
    std::cout << "Array size: " << N << " elements\n";
    std::cout << "Clamp range: [" << LO << ", " << HI << "]\n\n";
    
    // Create random data - values that will hit all three cases
    std::cout << "Generating random data...\n";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    std::vector<float> original(N);
    for (size_t i = 0; i < N; ++i) {
        original[i] = dist(gen);
    }
    
    // Count how many values fall in each category
    size_t below = 0, within = 0, above = 0;
    for (float x : original) {
        if (x < LO) below++;
        else if (x > HI) above++;
        else within++;
    }
    
    std::cout << "Data distribution:\n";
    std::cout << "  Below range: " << below << " (" << (100.0 * below / N) << "%)\n";
    std::cout << "  Within range: " << within << " (" << (100.0 * within / N) << "%)\n";
    std::cout << "  Above range: " << above << " (" << (100.0 * above / N) << "%)\n\n";
    
    // Working copy for each test
    std::vector<float> data;
    
    // Test 1: Branched version
    std::cout << "=== Test 1: Explicit if/else Branches ===\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            data = original;  // Fresh copy
            Timer timer;
            apply_clamp_branched(data, LO, HI);
            total_time += timer.elapsed_ms();
            sink_float = data[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Test 2: Ternary version
    std::cout << "=== Test 2: Ternary Operator ===\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            data = original;
            Timer timer;
            apply_clamp_ternary(data, LO, HI);
            total_time += timer.elapsed_ms();
            sink_float = data[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Test 3: min/max version
    std::cout << "=== Test 3: std::min/std::max ===\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            data = original;
            Timer timer;
            apply_clamp_minmax(data, LO, HI);
            total_time += timer.elapsed_ms();
            sink_float = data[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Test 4: std::clamp version
    std::cout << "=== Test 4: std::clamp (C++17) ===\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            data = original;
            Timer timer;
            apply_clamp_std(data, LO, HI);
            total_time += timer.elapsed_ms();
            sink_float = data[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Test 5: std::transform version
    std::cout << "=== Test 5: std::transform with std::clamp ===\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            data = original;
            Timer timer;
            apply_clamp_transform(data, LO, HI);
            total_time += timer.elapsed_ms();
            sink_float = data[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Verify correctness
    std::cout << "=== Correctness Check ===\n";
    data = original;
    apply_clamp_branched(data, LO, HI);
    
    std::vector<float> data2 = original;
    apply_clamp_std(data2, LO, HI);
    
    bool correct = true;
    for (size_t i = 0; i < N; ++i) {
        if (std::abs(data[i] - data2[i]) > 1e-6f) {
            correct = false;
            break;
        }
    }
    std::cout << "All implementations produce same result: " 
              << (correct ? "YES" : "NO") << "\n\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Branchless code techniques:\n\n";
    
    std::cout << "1. std::min / std::max:\n";
    std::cout << "   - Compiler often generates CMOV (conditional move)\n";
    std::cout << "   - No branch misprediction possible with CMOV\n";
    std::cout << "   - Works for any comparable type\n\n";
    
    std::cout << "2. std::clamp (C++17):\n";
    std::cout << "   - Standard library implementation\n";
    std::cout << "   - Typically uses min/max internally\n";
    std::cout << "   - Most readable and maintainable\n\n";
    
    std::cout << "3. Ternary operator:\n";
    std::cout << "   - Sometimes generates better code than if/else\n";
    std::cout << "   - Compiler may still use branches\n\n";
    
    std::cout << "4. Bit manipulation (for integers):\n";
    std::cout << "   - Guaranteed branchless\n";
    std::cout << "   - Complex and error-prone\n";
    std::cout << "   - Use only when absolutely necessary\n\n";
    
    std::cout << "To verify branchless code generation:\n";
    std::cout << "  g++ -S -O3 -std=c++17 02_branchless_clamp.cpp -o clamp.s\n";
    std::cout << "  Look for: cmov (conditional move) or minss/maxss (SSE)\n";
    std::cout << "  Avoid: je, jne, jl, jg (conditional jumps)\n";
    
    return 0;
}
