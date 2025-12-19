/**
 * Preventing Alias Analysis Failure (Pointer Aliasing and Vectorization)
 * 
 * This code demonstrates how pointer aliasing can prevent the compiler from
 * vectorizing code, and how to use __restrict__ to enable safe vectorization.
 * 
 * Key concepts:
 * - Pointer aliasing: Two pointers might point to overlapping memory
 * - When aliasing is possible, compiler must be conservative (no SIMD)
 * - __restrict__ tells compiler pointers don't overlap
 * - With __restrict__, compiler can safely vectorize (use SIMD)
 * 
 * The classic example: C[i] = A[i] + B[i]
 * - If A and C overlap, writing C[i] might change A[i+1]
 * - Compiler must process elements one-by-one (safe but slow)
 * - With __restrict__, compiler knows no overlap, can use SIMD
 * 
 * Compile: g++ -O3 -std=c++17 -march=native -o 02_alias 02_pointer_aliasing.cpp
 * 
 * To see vectorization reports:
 *   g++ -O3 -std=c++17 -march=native -fopt-info-vec-all 02_pointer_aliasing.cpp
 * 
 * Run: ./02_alias
 */

#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <cstring>
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
};

// Prevent compiler from optimizing away results
volatile float sink_float;

/**
 * Version 1: Potentially aliased pointers
 * 
 * The compiler doesn't know if A, B, C point to overlapping memory.
 * For safety, it must assume they MIGHT overlap.
 * 
 * Example of problematic aliasing:
 *   float data[10];
 *   add_arrays(data, data+1, data, 9);  // A and C overlap!
 *   
 * If the compiler vectorized this naively, it would compute wrong results.
 * So it processes elements one at a time (scalar code).
 */
void add_arrays_aliased(const float* A, const float* B, float* C, size_t n) {
    // Compiler cannot safely vectorize this loop
    // because A, B, C might point to overlapping memory
    for (size_t i = 0; i < n; ++i) {
        C[i] = A[i] + B[i];
        // If C[i] and A[i+1] overlap, this write affects the next read!
    }
}

/**
 * Version 2: Using __restrict__ to promise no aliasing
 * 
 * __restrict__ is a compiler hint (from C99, available as extension in C++)
 * It tells the compiler: "I promise these pointers don't overlap"
 * 
 * With this promise, the compiler can:
 * - Load multiple A[i], B[i] values at once (SIMD)
 * - Compute multiple additions in parallel
 * - Store multiple C[i] values at once
 * 
 * WARNING: If you lie (pass overlapping pointers), behavior is undefined!
 */
void add_arrays_restricted(const float* __restrict__ A, 
                           const float* __restrict__ B, 
                           float* __restrict__ C, 
                           size_t n) {
    // Compiler CAN vectorize this loop
    // because we promised A, B, C don't overlap
    for (size_t i = 0; i < n; ++i) {
        C[i] = A[i] + B[i];
        // Compiler knows C[i] write doesn't affect A or B reads
    }
}

/**
 * Version 3: Using local variables to help vectorization
 * 
 * Another technique: copy pointers to local restrict variables.
 * This can help in functions where you can't change the signature.
 */
void add_arrays_local_restrict(const float* A, const float* B, float* C, size_t n) {
    // Create local restricted pointers
    const float* __restrict__ rA = A;
    const float* __restrict__ rB = B;
    float* __restrict__ rC = C;
    
    for (size_t i = 0; i < n; ++i) {
        rC[i] = rA[i] + rB[i];
    }
}

/**
 * Version 4: Using std::vector (usually no aliasing issues)
 * 
 * When using separate std::vectors, the compiler often knows
 * they don't alias because they're separate allocations.
 */
void add_vectors(const std::vector<float>& A, 
                 const std::vector<float>& B, 
                 std::vector<float>& C) {
    size_t n = A.size();
    // Compiler may vectorize this because vectors are separate objects
    for (size_t i = 0; i < n; ++i) {
        C[i] = A[i] + B[i];
    }
}

/**
 * A more complex example: Scaling and accumulation
 * This shows aliasing issues in a more realistic scenario
 */
void scale_and_add_aliased(float* data, const float* scale, float* result, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        result[i] = data[i] * scale[i] + result[i];
        // If data and result overlap, this is sequential
    }
}

void scale_and_add_restricted(float* __restrict__ data, 
                              const float* __restrict__ scale, 
                              float* __restrict__ result, 
                              size_t n) {
    for (size_t i = 0; i < n; ++i) {
        result[i] = data[i] * scale[i] + result[i];
        // Can be vectorized with __restrict__
    }
}

/**
 * Benchmark helper
 */
template<typename Func>
double benchmark(Func&& func, int warmup_runs, int measure_runs) {
    // Warm-up
    for (int i = 0; i < warmup_runs; ++i) {
        func();
    }
    
    // Measure
    double total = 0;
    for (int i = 0; i < measure_runs; ++i) {
        Timer timer;
        func();
        total += timer.elapsed_ms();
    }
    
    return total / measure_runs;
}

int main() {
    std::cout << "=== Pointer Aliasing and Vectorization Demo ===\n\n";
    
    // Configuration
    const size_t N = 10000000;  // 10 million elements
    const int warmup_runs = 3;
    const int measure_runs = 10;
    
    std::cout << "Array size: " << N << " elements\n";
    std::cout << "Memory per array: " << (N * sizeof(float)) / (1024.0 * 1024.0) << " MB\n\n";
    
    // Allocate aligned memory for better SIMD performance
    // Using vectors ensures proper alignment
    std::vector<float> A(N), B(N), C(N);
    
    // Initialize with some values
    for (size_t i = 0; i < N; ++i) {
        A[i] = static_cast<float>(i) * 0.001f;
        B[i] = static_cast<float>(i) * 0.002f;
        C[i] = 0.0f;
    }
    
    // Get raw pointers
    float* pA = A.data();
    float* pB = B.data();
    float* pC = C.data();
    
    std::cout << "=== Benchmarking Array Addition ===\n\n";
    
    // Test 1: Potentially aliased version
    std::cout << "--- Test 1: Without __restrict__ (potential aliasing) ---\n";
    {
        double avg_time = benchmark([&]() {
            add_arrays_aliased(pA, pB, pC, N);
            sink_float = pC[N/2];  // Prevent optimization
        }, warmup_runs, measure_runs);
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "Sample result: C[" << N/2 << "] = " << pC[N/2] << "\n\n";
    }
    
    // Reset C
    std::fill(C.begin(), C.end(), 0.0f);
    
    // Test 2: With __restrict__
    std::cout << "--- Test 2: With __restrict__ (no aliasing guaranteed) ---\n";
    {
        double avg_time = benchmark([&]() {
            add_arrays_restricted(pA, pB, pC, N);
            sink_float = pC[N/2];
        }, warmup_runs, measure_runs);
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "Sample result: C[" << N/2 << "] = " << pC[N/2] << "\n\n";
    }
    
    // Reset C
    std::fill(C.begin(), C.end(), 0.0f);
    
    // Test 3: Local restrict variables
    std::cout << "--- Test 3: Local __restrict__ variables ---\n";
    {
        double avg_time = benchmark([&]() {
            add_arrays_local_restrict(pA, pB, pC, N);
            sink_float = pC[N/2];
        }, warmup_runs, measure_runs);
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "Sample result: C[" << N/2 << "] = " << pC[N/2] << "\n\n";
    }
    
    // Reset C
    std::fill(C.begin(), C.end(), 0.0f);
    
    // Test 4: Using std::vector
    std::cout << "--- Test 4: Using std::vector (compiler may auto-detect no alias) ---\n";
    {
        double avg_time = benchmark([&]() {
            add_vectors(A, B, C);
            sink_float = C[N/2];
        }, warmup_runs, measure_runs);
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << avg_time << " ms\n";
        std::cout << "Sample result: C[" << N/2 << "] = " << C[N/2] << "\n\n";
    }
    
    // Demonstration of actual aliasing problem
    std::cout << "=== Demonstration: Why Aliasing Matters ===\n\n";
    {
        // Create a small array where we intentionally alias
        std::vector<float> data = {1, 2, 3, 4, 5, 6, 7, 8};
        
        std::cout << "Original data: ";
        for (auto x : data) std::cout << x << " ";
        std::cout << "\n";
        
        // Create aliased pointers: A starts at index 0, C starts at index 1
        // So C[i] = A[i] + B[i] means:
        // C[0] (data[1]) = A[0] (data[0]) + B[0]
        // C[1] (data[2]) = A[1] (data[1]) + B[1]  <- A[1] was just modified!
        
        std::vector<float> B_small = {10, 10, 10, 10, 10, 10, 10, 10};
        
        // This is the ALIASED case: A and C point to overlapping regions
        float* aliased_A = data.data();      // Points to data[0]
        float* aliased_C = data.data() + 1;  // Points to data[1]
        
        std::cout << "\nWith aliasing (A starts at data[0], C starts at data[1]):\n";
        std::cout << "Each write to C[i] changes what A[i+1] reads!\n";
        
        // This must be done sequentially to be correct
        for (size_t i = 0; i < 6; ++i) {
            aliased_C[i] = aliased_A[i] + B_small[i];
        }
        
        std::cout << "Result: ";
        for (auto x : data) std::cout << x << " ";
        std::cout << "\n";
        
        std::cout << "\nNotice how the results propagate through the array!\n";
        std::cout << "This is why compilers must be conservative without __restrict__.\n";
    }
    
    // Summary
    std::cout << "\n=== Summary ===\n\n";
    
    std::cout << "Key Points:\n";
    std::cout << "1. Pointer aliasing prevents vectorization (SIMD)\n";
    std::cout << "2. __restrict__ promises no aliasing, enabling SIMD\n";
    std::cout << "3. Using __restrict__ incorrectly leads to undefined behavior\n";
    std::cout << "4. std::vector often helps compiler detect no aliasing\n\n";
    
    std::cout << "To see vectorization reports:\n";
    std::cout << "  GCC:   g++ -O3 -march=native -fopt-info-vec-all ...\n";
    std::cout << "  Clang: clang++ -O3 -march=native -Rpass=loop-vectorize ...\n\n";
    
    std::cout << "To inspect assembly:\n";
    std::cout << "  g++ -S -O3 -march=native 02_pointer_aliasing.cpp\n";
    std::cout << "  Look for SIMD instructions: vmovaps, vaddps, vmulps (AVX)\n";
    std::cout << "  Or: movaps, addps, mulps (SSE)\n";
    
    return 0;
}
