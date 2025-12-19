/*
 * 01_auto_vectorization.cpp
 * 
 * CONCEPT: Auto-Vectorization Check
 * TECHNIQUE: Enabling Auto-Vectorization
 * 
 * This program demonstrates how modern compilers can automatically convert
 * scalar loops into SIMD (Single Instruction Multiple Data) operations.
 * SIMD allows processing multiple data elements with a single instruction,
 * dramatically improving throughput for data-parallel workloads.
 * 
 * The compiler will generate AVX/SSE instructions when compiled with
 * appropriate flags like -O3 -march=native.
 * 
 * COMPILATION:
 *   # With vectorization report (GCC):
 *   g++ -O3 -march=native -fopt-info-vec-all -o 01_auto_vectorization 01_auto_vectorization.cpp
 * 
 *   # With vectorization report (Clang):
 *   clang++ -O3 -march=native -Rpass=loop-vectorize -o 01_auto_vectorization 01_auto_vectorization.cpp
 * 
 *   # To see assembly:
 *   g++ -O3 -march=native -S -fverbose-asm 01_auto_vectorization.cpp
 * 
 * RUN:
 *   ./01_auto_vectorization
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <iomanip>

// Helper function to prevent compiler from optimizing away results
template<typename T>
void do_not_optimize(T& value) {
    // This assembly prevents the compiler from optimizing away the value
    asm volatile("" : "+r"(value) : : "memory");
}

/*
 * Simple array scaling function - prime candidate for auto-vectorization
 * 
 * This function multiplies each element by a factor. The compiler can
 * recognize this pattern and generate SIMD instructions that process
 * multiple floats simultaneously (4 with SSE, 8 with AVX, 16 with AVX-512).
 */
void scale_array(float* data, size_t size, float factor) {
    for (size_t i = 0; i < size; ++i) {
        data[i] *= factor;  // Simple operation, easily vectorized
    }
}

/*
 * Array addition function - another classic vectorization candidate
 * 
 * Adding two arrays element-wise is embarrassingly parallel
 * and will be vectorized by the compiler.
 */
void add_arrays(const float* a, const float* b, float* result, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        result[i] = a[i] + b[i];  // Independent operations per element
    }
}

/*
 * Dot product - demonstrates reduction vectorization
 * 
 * The compiler can vectorize this using horizontal add instructions
 * after multiplying vectors element-wise.
 */
float dot_product(const float* a, const float* b, size_t size) {
    float sum = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        sum += a[i] * b[i];  // Reduction pattern
    }
    return sum;
}

/*
 * Benchmark helper - times a function call
 */
template<typename Func>
double benchmark(Func f, int iterations) {
    // Warm-up phase
    for (int i = 0; i < 10; ++i) {
        f();
    }
    
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
    
    std::cout << "=== Auto-Vectorization Demonstration ===\n";
    std::cout << "Array size: " << SIZE << " elements\n";
    std::cout << "Data size: " << (SIZE * sizeof(float)) / (1024.0 * 1024.0) << " MB\n\n";
    
    // Allocate aligned memory for better SIMD performance
    // Modern compilers handle alignment automatically with std::vector
    std::vector<float> data(SIZE);
    std::vector<float> a(SIZE);
    std::vector<float> b(SIZE);
    std::vector<float> result(SIZE);
    
    // Initialize with some data
    for (size_t i = 0; i < SIZE; ++i) {
        data[i] = static_cast<float>(i % 1000) / 1000.0f;
        a[i] = static_cast<float>(i % 500) / 500.0f;
        b[i] = static_cast<float>(i % 250) / 250.0f;
    }
    
    // Benchmark array scaling
    double scale_time = benchmark([&]() {
        scale_array(data.data(), SIZE, 1.001f);
        do_not_optimize(data[0]);  // Prevent optimization
    }, ITERATIONS);
    
    std::cout << "1. Array Scaling:\n";
    std::cout << "   Time per call: " << std::fixed << std::setprecision(3) << scale_time << " ms\n";
    std::cout << "   Throughput: " << (SIZE * sizeof(float) / (1024.0 * 1024.0)) / (scale_time / 1000.0) << " MB/s\n\n";
    
    // Benchmark array addition
    double add_time = benchmark([&]() {
        add_arrays(a.data(), b.data(), result.data(), SIZE);
        do_not_optimize(result[0]);
    }, ITERATIONS);
    
    std::cout << "2. Array Addition:\n";
    std::cout << "   Time per call: " << add_time << " ms\n";
    std::cout << "   Throughput: " << (3 * SIZE * sizeof(float) / (1024.0 * 1024.0)) / (add_time / 1000.0) << " MB/s\n\n";
    
    // Benchmark dot product
    float dot_result = 0;
    double dot_time = benchmark([&]() {
        dot_result = dot_product(a.data(), b.data(), SIZE);
        do_not_optimize(dot_result);
    }, ITERATIONS);
    
    std::cout << "3. Dot Product:\n";
    std::cout << "   Time per call: " << dot_time << " ms\n";
    std::cout << "   Result: " << dot_result << "\n";
    std::cout << "   Throughput: " << (2 * SIZE * sizeof(float) / (1024.0 * 1024.0)) / (dot_time / 1000.0) << " MB/s\n\n";
    
    std::cout << "=== Tips ===\n";
    std::cout << "1. Compile with -O3 -march=native for best vectorization\n";
    std::cout << "2. Use -fopt-info-vec-all (GCC) to see vectorization reports\n";
    std::cout << "3. Check assembly for vmulps, vaddps (AVX) or mulps, addps (SSE)\n";
    
    return 0;
}
