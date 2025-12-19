/*
 * 03_standard_algorithms.cpp
 * 
 * CONCEPT: Good Pattern: Standard Algorithms
 * TECHNIQUE: Standard Library Optimization
 * 
 * This program demonstrates why using C++ standard library algorithms
 * is often better than hand-written loops:
 * 
 *   1. Compilers optimize standard algorithms aggressively
 *   2. Algorithms express intent clearly (easier to vectorize)
 *   3. Algorithm implementations are highly optimized by experts
 *   4. Reduces bugs from off-by-one errors, etc.
 * 
 * We compare hand-written loops against:
 *   - std::accumulate / std::reduce
 *   - std::transform
 *   - std::copy
 *   - std::fill
 *   - std::find / std::count
 * 
 * The standard algorithms are often easier for the compiler to vectorize
 * and optimize compared to equivalent hand-written code.
 * 
 * COMPILATION:
 *   g++ -O3 -march=native -o 03_standard_algorithms 03_standard_algorithms.cpp
 * 
 *   # To see vectorization:
 *   g++ -O3 -march=native -fopt-info-vec -o 03_standard_algorithms 03_standard_algorithms.cpp
 * 
 * RUN:
 *   ./03_standard_algorithms
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <execution>  // C++17 parallel algorithms (optional)

/*
 * DoNotOptimize: Prevent dead code elimination
 */
template<typename T>
void DoNotOptimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

// ==================== Sum of Elements ====================

/*
 * Hand-written sum loop
 */
long long handwritten_sum(const std::vector<int>& v) {
    long long sum = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        sum += v[i];
    }
    return sum;
}

/*
 * Range-based for sum
 */
long long rangefor_sum(const std::vector<int>& v) {
    long long sum = 0;
    for (int x : v) {
        sum += x;
    }
    return sum;
}

/*
 * std::accumulate (serial)
 */
long long accumulate_sum(const std::vector<int>& v) {
    return std::accumulate(v.begin(), v.end(), 0LL);
}

/*
 * std::reduce (C++17, potentially parallel-friendly)
 */
long long reduce_sum(const std::vector<int>& v) {
    return std::reduce(v.begin(), v.end(), 0LL);
}

// ==================== Sum of Squares ====================

/*
 * Hand-written sum of squares
 */
long long handwritten_sum_squares(const std::vector<int>& v) {
    long long sum = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        sum += static_cast<long long>(v[i]) * v[i];
    }
    return sum;
}

/*
 * std::transform_reduce (C++17) - most expressive and optimizable
 */
long long transform_reduce_sum_squares(const std::vector<int>& v) {
    return std::transform_reduce(
        v.begin(), v.end(),
        0LL,                                    // Initial value
        std::plus<>(),                          // Reduction operation
        [](int x) { return static_cast<long long>(x) * x; }  // Transform
    );
}

/*
 * std::accumulate with custom operation
 */
long long accumulate_sum_squares(const std::vector<int>& v) {
    return std::accumulate(v.begin(), v.end(), 0LL,
        [](long long acc, int x) { return acc + static_cast<long long>(x) * x; });
}

// ==================== Transform (Scale Array) ====================

/*
 * Hand-written array scaling
 */
void handwritten_scale(std::vector<double>& v, double factor) {
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] *= factor;
    }
}

/*
 * std::transform (in-place)
 */
void transform_scale(std::vector<double>& v, double factor) {
    std::transform(v.begin(), v.end(), v.begin(),
        [factor](double x) { return x * factor; });
}

/*
 * std::for_each
 */
void foreach_scale(std::vector<double>& v, double factor) {
    std::for_each(v.begin(), v.end(),
        [factor](double& x) { x *= factor; });
}

// ==================== Find / Count ====================

/*
 * Hand-written count
 */
int handwritten_count(const std::vector<int>& v, int target) {
    int count = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] == target) {
            ++count;
        }
    }
    return count;
}

/*
 * std::count
 */
int algorithm_count(const std::vector<int>& v, int target) {
    return static_cast<int>(std::count(v.begin(), v.end(), target));
}

/*
 * Hand-written count_if (greater than threshold)
 */
int handwritten_count_if(const std::vector<int>& v, int threshold) {
    int count = 0;
    for (size_t i = 0; i < v.size(); ++i) {
        if (v[i] > threshold) {
            ++count;
        }
    }
    return count;
}

/*
 * std::count_if
 */
int algorithm_count_if(const std::vector<int>& v, int threshold) {
    return static_cast<int>(std::count_if(v.begin(), v.end(),
        [threshold](int x) { return x > threshold; }));
}

// ==================== Copy / Fill ====================

/*
 * Hand-written copy
 */
void handwritten_copy(const std::vector<int>& src, std::vector<int>& dst) {
    for (size_t i = 0; i < src.size(); ++i) {
        dst[i] = src[i];
    }
}

/*
 * std::copy
 */
void algorithm_copy(const std::vector<int>& src, std::vector<int>& dst) {
    std::copy(src.begin(), src.end(), dst.begin());
}

/*
 * Hand-written fill
 */
void handwritten_fill(std::vector<int>& v, int value) {
    for (size_t i = 0; i < v.size(); ++i) {
        v[i] = value;
    }
}

/*
 * std::fill
 */
void algorithm_fill(std::vector<int>& v, int value) {
    std::fill(v.begin(), v.end(), value);
}

// ==================== Benchmark ====================

template<typename Func>
double benchmark(Func f, int runs = 10) {
    // Warm-up
    for (int i = 0; i < 3; ++i) f();
    
    double total = 0;
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        f();
        auto end = std::chrono::high_resolution_clock::now();
        total += std::chrono::duration<double, std::milli>(end - start).count();
    }
    
    return total / runs;
}

int main() {
    std::cout << "=== Standard Algorithms vs Hand-Written Loops ===\n\n";
    std::cout << std::fixed << std::setprecision(3);
    
    const size_t SIZE = 10'000'000;
    
    // Initialize test data
    std::vector<int> int_data(SIZE);
    std::vector<double> double_data(SIZE);
    for (size_t i = 0; i < SIZE; ++i) {
        int_data[i] = static_cast<int>(i % 1000);
        double_data[i] = static_cast<double>(i) / SIZE;
    }
    
    std::cout << "Data size: " << SIZE << " elements\n\n";
    
    // ========== Test 1: Sum ===========
    std::cout << "--- Test 1: Sum of Elements ---\n";
    
    long long sum_result = 0;
    
    double hw_sum = benchmark([&]() {
        sum_result = handwritten_sum(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "Hand-written loop:  " << hw_sum << " ms (sum=" << sum_result << ")\n";
    
    double rf_sum = benchmark([&]() {
        sum_result = rangefor_sum(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "Range-for loop:     " << rf_sum << " ms\n";
    
    double acc_sum = benchmark([&]() {
        sum_result = accumulate_sum(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "std::accumulate:    " << acc_sum << " ms\n";
    
    double red_sum = benchmark([&]() {
        sum_result = reduce_sum(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "std::reduce:        " << red_sum << " ms\n\n";
    
    // ========== Test 2: Sum of Squares ===========
    std::cout << "--- Test 2: Sum of Squares ---\n";
    
    double hw_sq = benchmark([&]() {
        sum_result = handwritten_sum_squares(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "Hand-written:           " << hw_sq << " ms (sum=" << sum_result << ")\n";
    
    double acc_sq = benchmark([&]() {
        sum_result = accumulate_sum_squares(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "std::accumulate:        " << acc_sq << " ms\n";
    
    double tr_sq = benchmark([&]() {
        sum_result = transform_reduce_sum_squares(int_data);
        DoNotOptimize(sum_result);
    });
    std::cout << "std::transform_reduce:  " << tr_sq << " ms\n\n";
    
    // ========== Test 3: Array Scaling ===========
    std::cout << "--- Test 3: Array Scaling (Transform) ---\n";
    
    std::vector<double> temp = double_data;  // Work on copy
    
    double hw_scale = benchmark([&]() {
        temp = double_data;
        handwritten_scale(temp, 1.5);
        DoNotOptimize(temp[0]);
    });
    std::cout << "Hand-written:      " << hw_scale << " ms\n";
    
    double tr_scale = benchmark([&]() {
        temp = double_data;
        transform_scale(temp, 1.5);
        DoNotOptimize(temp[0]);
    });
    std::cout << "std::transform:    " << tr_scale << " ms\n";
    
    double fe_scale = benchmark([&]() {
        temp = double_data;
        foreach_scale(temp, 1.5);
        DoNotOptimize(temp[0]);
    });
    std::cout << "std::for_each:     " << fe_scale << " ms\n\n";
    
    // ========== Test 4: Count ===========
    std::cout << "--- Test 4: Count Operations ---\n";
    
    int count_result = 0;
    
    double hw_count = benchmark([&]() {
        count_result = handwritten_count(int_data, 500);
        DoNotOptimize(count_result);
    });
    std::cout << "Hand-written count:     " << hw_count << " ms (count=" << count_result << ")\n";
    
    double alg_count = benchmark([&]() {
        count_result = algorithm_count(int_data, 500);
        DoNotOptimize(count_result);
    });
    std::cout << "std::count:             " << alg_count << " ms\n";
    
    double hw_count_if = benchmark([&]() {
        count_result = handwritten_count_if(int_data, 500);
        DoNotOptimize(count_result);
    });
    std::cout << "Hand-written count_if:  " << hw_count_if << " ms (count=" << count_result << ")\n";
    
    double alg_count_if = benchmark([&]() {
        count_result = algorithm_count_if(int_data, 500);
        DoNotOptimize(count_result);
    });
    std::cout << "std::count_if:          " << alg_count_if << " ms\n\n";
    
    // ========== Test 5: Copy/Fill ===========
    std::cout << "--- Test 5: Copy and Fill ---\n";
    
    std::vector<int> dst(SIZE);
    
    double hw_copy = benchmark([&]() {
        handwritten_copy(int_data, dst);
        DoNotOptimize(dst[0]);
    });
    std::cout << "Hand-written copy:  " << hw_copy << " ms\n";
    
    double alg_copy = benchmark([&]() {
        algorithm_copy(int_data, dst);
        DoNotOptimize(dst[0]);
    });
    std::cout << "std::copy:          " << alg_copy << " ms\n";
    
    double hw_fill = benchmark([&]() {
        handwritten_fill(dst, 42);
        DoNotOptimize(dst[0]);
    });
    std::cout << "Hand-written fill:  " << hw_fill << " ms\n";
    
    double alg_fill = benchmark([&]() {
        algorithm_fill(dst, 42);
        DoNotOptimize(dst[0]);
    });
    std::cout << "std::fill:          " << alg_fill << " ms\n\n";
    
    // ========== Summary ===========
    std::cout << "=== Why Use Standard Algorithms? ===\n\n";
    
    std::cout << "1. COMPILER OPTIMIZATION:\n";
    std::cout << "   - Compilers recognize standard algorithms\n";
    std::cout << "   - Can apply special optimizations (vectorization, unrolling)\n";
    std::cout << "   - Better alias analysis for standard containers\n\n";
    
    std::cout << "2. CORRECTNESS:\n";
    std::cout << "   - No off-by-one errors\n";
    std::cout << "   - No iterator invalidation mistakes\n";
    std::cout << "   - Algorithms handle edge cases\n\n";
    
    std::cout << "3. READABILITY:\n";
    std::cout << "   - Intent is clear from algorithm name\n";
    std::cout << "   - Less code to read and maintain\n";
    std::cout << "   - Familiar patterns for C++ developers\n\n";
    
    std::cout << "4. PARALLEL EXECUTION (C++17):\n";
    std::cout << "   - std::reduce, std::transform_reduce support parallelism\n";
    std::cout << "   - Add std::execution::par for parallel execution\n";
    std::cout << "   - Hand-written loops need manual threading\n\n";
    
    std::cout << "=== Recommended Algorithms ===\n\n";
    std::cout << "Aggregation:  std::accumulate, std::reduce, std::transform_reduce\n";
    std::cout << "Transform:    std::transform, std::for_each\n";
    std::cout << "Search:       std::find, std::find_if, std::binary_search\n";
    std::cout << "Count:        std::count, std::count_if\n";
    std::cout << "Min/Max:      std::min_element, std::max_element, std::minmax\n";
    std::cout << "Sorting:      std::sort, std::stable_sort, std::partial_sort\n";
    std::cout << "Copy/Move:    std::copy, std::move, std::fill\n";
    
    return 0;
}
