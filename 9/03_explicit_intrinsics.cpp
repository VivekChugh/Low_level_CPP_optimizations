/*
 * 03_explicit_intrinsics.cpp
 * 
 * CONCEPT: Explicit Intrinsics Implementation
 * TECHNIQUE: Manual SIMD
 * 
 * This program demonstrates the use of explicit SIMD intrinsics to manually
 * vectorize code. While compilers can auto-vectorize many loops, there are
 * cases where manual SIMD provides:
 *   1. Guaranteed vectorization (no compiler guessing)
 *   2. Fine-grained control over instruction selection
 *   3. Ability to use specialized instructions
 * 
 * We use AVX (Advanced Vector Extensions) intrinsics which operate on
 * 256-bit registers (8 floats or 4 doubles simultaneously).
 * 
 * IMPORTANT: This code requires a CPU with AVX support (most CPUs since 2011).
 * 
 * COMPILATION:
 *   # GCC/Clang:
 *   g++ -O3 -mavx -o 03_explicit_intrinsics 03_explicit_intrinsics.cpp
 * 
 *   # With native optimizations:
 *   g++ -O3 -march=native -o 03_explicit_intrinsics 03_explicit_intrinsics.cpp
 * 
 * RUN:
 *   ./03_explicit_intrinsics
 * 
 * NOTE: If your CPU doesn't support AVX, compile with -msse instead and
 *       modify the code to use SSE intrinsics (_mm_...).
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <cstring>

// Include SIMD intrinsics header
#include <immintrin.h>  // For AVX intrinsics

// Alignment requirement for AVX (32 bytes)
constexpr size_t AVX_ALIGNMENT = 32;

// Prevent dead code elimination
template<typename T>
void do_not_optimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

/*
 * Scalar implementation of array scaling
 * 
 * This is the baseline: one multiplication per iteration.
 * The compiler may auto-vectorize this, but we'll compare
 * against our explicit SIMD version.
 */
void scale_scalar(float* data, size_t size, float factor) {
    for (size_t i = 0; i < size; ++i) {
        data[i] *= factor;
    }
}

/*
 * AVX implementation of array scaling
 * 
 * Uses 256-bit registers to process 8 floats at once.
 * This is 8x more work per instruction (theoretically).
 * 
 * Key intrinsics used:
 *   _mm256_set1_ps(x)   - Broadcast single float to all 8 positions
 *   _mm256_load_ps(p)   - Load 8 floats from aligned memory
 *   _mm256_mul_ps(a,b)  - Multiply 8 floats in parallel
 *   _mm256_store_ps(p,v)- Store 8 floats to aligned memory
 */
void scale_avx(float* data, size_t size, float factor) {
    // Broadcast the scaling factor to all 8 lanes of a 256-bit register
    // [factor, factor, factor, factor, factor, factor, factor, factor]
    __m256 vfactor = _mm256_set1_ps(factor);
    
    // Process 8 elements at a time
    size_t i = 0;
    size_t vec_size = size - (size % 8);  // Round down to multiple of 8
    
    for (; i < vec_size; i += 8) {
        // Load 8 floats from memory into a 256-bit register
        __m256 vdata = _mm256_load_ps(&data[i]);
        
        // Multiply all 8 floats in parallel
        __m256 vresult = _mm256_mul_ps(vdata, vfactor);
        
        // Store the 8 results back to memory
        _mm256_store_ps(&data[i], vresult);
    }
    
    // Handle remaining elements (tail) with scalar code
    for (; i < size; ++i) {
        data[i] *= factor;
    }
}

/*
 * Scalar dot product
 * 
 * Standard implementation: multiply and accumulate.
 */
float dot_product_scalar(const float* a, const float* b, size_t size) {
    float sum = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

/*
 * AVX dot product
 * 
 * Uses horizontal addition to sum partial results.
 * 
 * Key intrinsics:
 *   _mm256_setzero_ps()     - Create a register of zeros
 *   _mm256_fmadd_ps(a,b,c)  - Fused multiply-add: a*b + c (requires FMA)
 *   _mm256_hadd_ps(a,b)     - Horizontal add pairs within register
 *   _mm_add_ps(a,b)         - 128-bit add (for final reduction)
 */
float dot_product_avx(const float* a, const float* b, size_t size) {
    // Accumulator for partial sums (8 parallel accumulators)
    __m256 vsum = _mm256_setzero_ps();
    
    size_t i = 0;
    size_t vec_size = size - (size % 8);
    
    for (; i < vec_size; i += 8) {
        // Load 8 elements from each array
        __m256 va = _mm256_load_ps(&a[i]);
        __m256 vb = _mm256_load_ps(&b[i]);
        
        // Multiply and accumulate: vsum = va * vb + vsum
        // Using FMA (fused multiply-add) for better accuracy and speed
        #ifdef __FMA__
        vsum = _mm256_fmadd_ps(va, vb, vsum);
        #else
        __m256 vmul = _mm256_mul_ps(va, vb);
        vsum = _mm256_add_ps(vsum, vmul);
        #endif
    }
    
    // Horizontal sum of the 8 partial sums
    // Step 1: Add pairs horizontally [a0+a1, a2+a3, a4+a5, a6+a7, ...]
    __m256 hsum1 = _mm256_hadd_ps(vsum, vsum);
    // Step 2: Add pairs again
    __m256 hsum2 = _mm256_hadd_ps(hsum1, hsum1);
    
    // Extract the two 128-bit halves and add them
    __m128 lo = _mm256_extractf128_ps(hsum2, 0);
    __m128 hi = _mm256_extractf128_ps(hsum2, 1);
    __m128 final_sum = _mm_add_ps(lo, hi);
    
    // Extract the scalar result
    float result = _mm_cvtss_f32(final_sum);
    
    // Handle tail elements
    for (; i < size; ++i) {
        result += a[i] * b[i];
    }
    
    return result;
}

/*
 * Allocate aligned memory for SIMD operations
 * 
 * SIMD instructions often require or perform better with aligned memory.
 * AVX requires 32-byte alignment for optimal performance.
 */
float* allocate_aligned(size_t count) {
    void* ptr = nullptr;
    if (posix_memalign(&ptr, AVX_ALIGNMENT, count * sizeof(float)) != 0) {
        return nullptr;
    }
    return static_cast<float*>(ptr);
}

/*
 * Benchmark helper
 */
template<typename Func>
double benchmark(Func f, int iterations) {
    // Warm-up
    for (int i = 0; i < 10; ++i) f();
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        f();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

int main() {
    const size_t SIZE = 10'000'000;  // 10 million elements
    const int ITERATIONS = 100;
    
    std::cout << "=== Explicit SIMD Intrinsics Demonstration ===\n";
    std::cout << "Array size: " << SIZE << " elements\n";
    std::cout << "Using AVX (256-bit, 8 floats per operation)\n\n";
    
    // Allocate aligned memory for SIMD operations
    float* data_scalar = allocate_aligned(SIZE);
    float* data_avx = allocate_aligned(SIZE);
    float* arr_a = allocate_aligned(SIZE);
    float* arr_b = allocate_aligned(SIZE);
    
    if (!data_scalar || !data_avx || !arr_a || !arr_b) {
        std::cerr << "Failed to allocate aligned memory!\n";
        return 1;
    }
    
    // Initialize data
    for (size_t i = 0; i < SIZE; ++i) {
        float val = static_cast<float>(i % 1000) / 1000.0f;
        data_scalar[i] = val;
        data_avx[i] = val;
        arr_a[i] = val;
        arr_b[i] = (1000 - (i % 1000)) / 1000.0f;
    }
    
    // ========== Test 1: Array Scaling ==========
    std::cout << "--- Test 1: Array Scaling ---\n";
    
    double scalar_scale_time = benchmark([&]() {
        scale_scalar(data_scalar, SIZE, 1.001f);
        do_not_optimize(data_scalar[0]);
    }, ITERATIONS);
    
    double avx_scale_time = benchmark([&]() {
        scale_avx(data_avx, SIZE, 1.001f);
        do_not_optimize(data_avx[0]);
    }, ITERATIONS);
    
    std::cout << "Scalar time: " << std::fixed << std::setprecision(3) << scalar_scale_time << " ms\n";
    std::cout << "AVX time:    " << avx_scale_time << " ms\n";
    std::cout << "Speedup:     " << scalar_scale_time / avx_scale_time << "x\n\n";
    
    // ========== Test 2: Dot Product ==========
    std::cout << "--- Test 2: Dot Product ---\n";
    
    float result_scalar = 0, result_avx = 0;
    
    double scalar_dot_time = benchmark([&]() {
        result_scalar = dot_product_scalar(arr_a, arr_b, SIZE);
        do_not_optimize(result_scalar);
    }, ITERATIONS);
    
    double avx_dot_time = benchmark([&]() {
        result_avx = dot_product_avx(arr_a, arr_b, SIZE);
        do_not_optimize(result_avx);
    }, ITERATIONS);
    
    std::cout << "Scalar time:   " << scalar_dot_time << " ms (result: " << result_scalar << ")\n";
    std::cout << "AVX time:      " << avx_dot_time << " ms (result: " << result_avx << ")\n";
    std::cout << "Speedup:       " << scalar_dot_time / avx_dot_time << "x\n";
    std::cout << "Results match: " << (std::abs(result_scalar - result_avx) < 0.01f ? "Yes" : "No") << "\n\n";
    
    // ========== Summary ==========
    std::cout << "=== Key SIMD Intrinsics Used ===\n";
    std::cout << "_mm256_set1_ps(x)     - Broadcast scalar to all 8 lanes\n";
    std::cout << "_mm256_load_ps(ptr)   - Load 8 floats (aligned)\n";
    std::cout << "_mm256_store_ps(ptr)  - Store 8 floats (aligned)\n";
    std::cout << "_mm256_mul_ps(a, b)   - Multiply 8 float pairs\n";
    std::cout << "_mm256_add_ps(a, b)   - Add 8 float pairs\n";
    std::cout << "_mm256_fmadd_ps(a,b,c)- Fused multiply-add (a*b + c)\n";
    std::cout << "_mm256_hadd_ps(a, b)  - Horizontal add for reductions\n\n";
    
    std::cout << "=== Notes ===\n";
    std::cout << "1. Memory must be 32-byte aligned for AVX load/store\n";
    std::cout << "2. Handle tail elements with scalar code\n";
    std::cout << "3. Auto-vectorization often matches manual SIMD for simple loops\n";
    std::cout << "4. Manual SIMD shines for complex patterns compilers miss\n";
    
    // Cleanup
    free(data_scalar);
    free(data_avx);
    free(arr_a);
    free(arr_b);
    
    return 0;
}
