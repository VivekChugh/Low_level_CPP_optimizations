/*
 * 01_allocation_antipattern.cpp
 * 
 * CONCEPT: Anti-Pattern: Allocation in Hot Loop
 * TECHNIQUE: Avoiding Allocation Anti-Patterns
 * 
 * This program demonstrates one of the most common and costly performance
 * anti-patterns: allocating memory inside a frequently-executed (hot) loop.
 * 
 * Why is this bad?
 *   1. malloc/free are expensive system calls
 *   2. Memory allocators need to maintain bookkeeping data
 *   3. Allocated memory may not be in cache
 *   4. Multi-threaded allocators have contention
 *   5. Can cause memory fragmentation over time
 * 
 * This is a PRIMARY example of code that suffers from high latency and
 * fragmentation, as mentioned in the book.
 * 
 * COMPILATION:
 *   g++ -O3 -o 01_allocation_antipattern 01_allocation_antipattern.cpp
 * 
 * RUN:
 *   ./01_allocation_antipattern
 */

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <iomanip>
#include <cstring>

/*
 * DoNotOptimize: Prevent dead code elimination
 */
template<typename T>
void DoNotOptimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

// ==================== Anti-Patterns ====================

/*
 * ANTI-PATTERN 1: Vector created inside loop
 * 
 * For each of N iterations:
 *   - Allocate memory for vector
 *   - Initialize elements
 *   - Process elements
 *   - Deallocate memory
 * 
 * This means N allocations + N deallocations!
 */
long long antipattern_vector_in_loop(int iterations, int vector_size) {
    long long total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // BAD: Allocates memory every single iteration
        std::vector<int> temp(vector_size);
        
        // Fill with data
        for (int j = 0; j < vector_size; ++j) {
            temp[j] = i * vector_size + j;
        }
        
        // Process
        for (int val : temp) {
            total += val;
        }
        
        // temp is destroyed here, memory freed
    }
    
    return total;
}

/*
 * ANTI-PATTERN 2: String created inside loop
 * 
 * Similar problem - each string creation may allocate.
 * Small String Optimization (SSO) helps for small strings,
 * but larger strings always allocate.
 */
long long antipattern_string_in_loop(int iterations) {
    long long total_length = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // BAD: String allocation every iteration
        std::string message = "Processing iteration number " + 
                             std::to_string(i) + 
                             " which is a reasonably long string";
        
        total_length += message.length();
    }
    
    return total_length;
}

/*
 * ANTI-PATTERN 3: Unique_ptr/make_unique in loop
 * 
 * Smart pointers don't solve allocation overhead -
 * they just manage ownership. The allocation still happens.
 */
long long antipattern_unique_ptr_in_loop(int iterations, int array_size) {
    long long total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // BAD: Heap allocation every iteration
        auto data = std::make_unique<int[]>(array_size);
        
        for (int j = 0; j < array_size; ++j) {
            data[j] = i + j;
        }
        
        for (int j = 0; j < array_size; ++j) {
            total += data[j];
        }
        
        // data deleted here
    }
    
    return total;
}

/*
 * ANTI-PATTERN 4: malloc/free in loop
 * 
 * Raw C-style allocation has the same problem.
 */
long long antipattern_malloc_in_loop(int iterations, int array_size) {
    long long total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // BAD: System call every iteration
        int* data = static_cast<int*>(malloc(array_size * sizeof(int)));
        
        for (int j = 0; j < array_size; ++j) {
            data[j] = i + j;
        }
        
        for (int j = 0; j < array_size; ++j) {
            total += data[j];
        }
        
        free(data);  // System call again
    }
    
    return total;
}

// ==================== Fixed Versions ====================

/*
 * FIXED 1: Vector allocated once, reused
 */
long long fixed_vector_reuse(int iterations, int vector_size) {
    long long total = 0;
    
    // GOOD: Allocate once outside the loop
    std::vector<int> temp(vector_size);
    
    for (int i = 0; i < iterations; ++i) {
        // Fill with data (no allocation)
        for (int j = 0; j < vector_size; ++j) {
            temp[j] = i * vector_size + j;
        }
        
        // Process
        for (int val : temp) {
            total += val;
        }
        
        // Vector persists, no deallocation
    }
    
    return total;
}

/*
 * FIXED 2: String buffer reused
 */
long long fixed_string_reuse(int iterations) {
    long long total_length = 0;
    
    // GOOD: Reuse string buffer
    std::string message;
    message.reserve(100);  // Pre-allocate capacity
    
    for (int i = 0; i < iterations; ++i) {
        message.clear();  // Clear but keep capacity
        
        message = "Processing iteration number ";
        message += std::to_string(i);
        message += " which is a reasonably long string";
        
        total_length += message.length();
    }
    
    return total_length;
}

/*
 * FIXED 3: Stack allocation for small, fixed-size arrays
 */
long long fixed_stack_allocation(int iterations, int array_size) {
    long long total = 0;
    
    // GOOD: Stack allocation (no heap overhead)
    // Only works for small, compile-time-known sizes
    constexpr int STACK_SIZE = 1000;
    int data[STACK_SIZE];
    
    // Ensure we don't exceed stack array
    int effective_size = std::min(array_size, STACK_SIZE);
    
    for (int i = 0; i < iterations; ++i) {
        for (int j = 0; j < effective_size; ++j) {
            data[j] = i + j;
        }
        
        for (int j = 0; j < effective_size; ++j) {
            total += data[j];
        }
    }
    
    return total;
}

// ==================== Benchmark ====================

template<typename Func>
double benchmark(const char* name, Func f, int runs = 5) {
    // Warm-up
    for (int i = 0; i < 2; ++i) {
        auto result = f();
        DoNotOptimize(result);
    }
    
    double total = 0;
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = f();
        DoNotOptimize(result);
        auto end = std::chrono::high_resolution_clock::now();
        total += std::chrono::duration<double, std::milli>(end - start).count();
    }
    
    return total / runs;
}

int main() {
    std::cout << "=== Allocation in Hot Loop Anti-Pattern ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    const int ITERATIONS = 100000;
    const int VECTOR_SIZE = 1000;
    
    // ========== Test 1: Vector in loop ==========
    std::cout << "--- Test 1: Vector Allocation ---\n";
    std::cout << "Iterations: " << ITERATIONS << ", Vector size: " << VECTOR_SIZE << "\n\n";
    
    double anti1 = benchmark("Antipattern", [&]() {
        return antipattern_vector_in_loop(ITERATIONS, VECTOR_SIZE);
    });
    std::cout << "Anti-pattern (alloc in loop):   " << anti1 << " ms\n";
    
    double fixed1 = benchmark("Fixed", [&]() {
        return fixed_vector_reuse(ITERATIONS, VECTOR_SIZE);
    });
    std::cout << "Fixed (reuse allocation):       " << fixed1 << " ms\n";
    std::cout << "Speedup: " << anti1 / fixed1 << "x\n\n";
    
    // ========== Test 2: String in loop ==========
    std::cout << "--- Test 2: String Allocation ---\n";
    std::cout << "Iterations: " << ITERATIONS << "\n\n";
    
    double anti2 = benchmark("Antipattern", [&]() {
        return antipattern_string_in_loop(ITERATIONS);
    });
    std::cout << "Anti-pattern (string in loop):  " << anti2 << " ms\n";
    
    double fixed2 = benchmark("Fixed", [&]() {
        return fixed_string_reuse(ITERATIONS);
    });
    std::cout << "Fixed (reuse string buffer):    " << fixed2 << " ms\n";
    std::cout << "Speedup: " << anti2 / fixed2 << "x\n\n";
    
    // ========== Test 3: unique_ptr/malloc comparison ==========
    std::cout << "--- Test 3: Heap vs Stack Allocation ---\n";
    std::cout << "Iterations: " << ITERATIONS << ", Array size: " << VECTOR_SIZE << "\n\n";
    
    double anti3a = benchmark("unique_ptr", [&]() {
        return antipattern_unique_ptr_in_loop(ITERATIONS, VECTOR_SIZE);
    });
    std::cout << "unique_ptr (alloc in loop):     " << anti3a << " ms\n";
    
    double anti3b = benchmark("malloc", [&]() {
        return antipattern_malloc_in_loop(ITERATIONS, VECTOR_SIZE);
    });
    std::cout << "malloc (alloc in loop):         " << anti3b << " ms\n";
    
    double fixed3 = benchmark("stack", [&]() {
        return fixed_stack_allocation(ITERATIONS, VECTOR_SIZE);
    });
    std::cout << "Stack allocation (no heap):     " << fixed3 << " ms\n";
    std::cout << "Speedup vs unique_ptr: " << anti3a / fixed3 << "x\n\n";
    
    // ========== Summary ==========
    std::cout << "=== Why Allocation in Loops is Bad ===\n\n";
    
    std::cout << "1. SYSTEM CALL OVERHEAD:\n";
    std::cout << "   - malloc/free involve kernel interaction\n";
    std::cout << "   - Thread-safe allocators have lock overhead\n";
    std::cout << "   - Memory management bookkeeping takes time\n\n";
    
    std::cout << "2. CACHE EFFECTS:\n";
    std::cout << "   - Newly allocated memory is likely not in cache\n";
    std::cout << "   - Repeated alloc/free patterns are cache-unfriendly\n";
    std::cout << "   - Fragmentation spreads data across memory\n\n";
    
    std::cout << "3. MEMORY FRAGMENTATION:\n";
    std::cout << "   - Many small allocations fragment the heap\n";
    std::cout << "   - Over time, allocator performance degrades\n";
    std::cout << "   - Memory usage may grow even without leaks\n\n";
    
    std::cout << "=== Solutions ===\n\n";
    std::cout << "1. Move allocation outside the loop\n";
    std::cout << "2. Use clear() + reuse instead of recreate\n";
    std::cout << "3. Use reserve() to pre-allocate capacity\n";
    std::cout << "4. Consider stack allocation for small, fixed arrays\n";
    std::cout << "5. Use object pools for frequently created/destroyed objects\n";
    std::cout << "6. Consider PMR allocators for scoped allocations\n";
    
    return 0;
}
