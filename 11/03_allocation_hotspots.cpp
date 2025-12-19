/*
 * 03_allocation_hotspots.cpp
 * 
 * CONCEPT: Allocations Hotspot Identification
 * TECHNIQUE: Profiling Tools
 * 
 * This program demonstrates techniques to identify memory allocation hotspots
 * in your code. Excessive allocations in hot paths can severely impact
 * performance due to:
 *   1. System call overhead (malloc/free)
 *   2. Memory fragmentation
 *   3. Cache pollution
 *   4. Contention in multi-threaded allocators
 * 
 * This file provides:
 *   - Example code with intentional allocation hotspots
 *   - Self-tracking allocation counter (for demonstration)
 *   - Instructions for using real profiling tools
 * 
 * COMPILATION:
 *   # Standard build:
 *   g++ -O3 -o 03_allocation_hotspots 03_allocation_hotspots.cpp
 * 
 *   # With debug symbols for profiling:
 *   g++ -O3 -g -o 03_allocation_hotspots 03_allocation_hotspots.cpp
 * 
 * RUN:
 *   ./03_allocation_hotspots
 * 
 * PROFILING (Linux):
 *   # Using Valgrind's Massif for heap profiling:
 *   valgrind --tool=massif ./03_allocation_hotspots
 *   ms_print massif.out.<pid>
 * 
 *   # Using Heaptrack (better visualization):
 *   heaptrack ./03_allocation_hotspots
 *   heaptrack_gui heaptrack.03_allocation_hotspots.<pid>.gz
 * 
 *   # Using perf for sampling:
 *   perf record -g ./03_allocation_hotspots
 *   perf report
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <memory>
#include <list>
#include <sstream>

/*
 * Simple allocation counter for demonstration
 * 
 * In production, you'd use tools like:
 *   - Valgrind/Massif
 *   - Heaptrack
 *   - AddressSanitizer/LeakSanitizer
 *   - Custom allocator wrappers
 */
struct AllocationTracker {
    static inline size_t allocation_count = 0;
    static inline size_t deallocation_count = 0;
    static inline size_t total_bytes_allocated = 0;
    
    static void reset() {
        allocation_count = 0;
        deallocation_count = 0;
        total_bytes_allocated = 0;
    }
    
    static void print() {
        std::cout << "  Allocations:   " << allocation_count << "\n";
        std::cout << "  Deallocations: " << deallocation_count << "\n";
        std::cout << "  Total bytes:   " << total_bytes_allocated << "\n";
    }
};

// Note: Overriding global new/delete affects ALL allocations including
// those made by the standard library. This is for demonstration only.
// In production, use profiling tools instead.

/*
 * DoNotOptimize: Prevent dead code elimination
 */
template<typename T>
void DoNotOptimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

// ==================== Anti-Patterns ====================

/*
 * ANTI-PATTERN 1: Allocation in a hot loop
 * 
 * Every iteration creates a new vector, allocates memory, then destroys it.
 * This is extremely wasteful when processing millions of items.
 */
void antipattern_alloc_in_loop(int iterations) {
    long long sum = 0;
    for (int i = 0; i < iterations; ++i) {
        // BAD: New allocation every iteration!
        std::vector<int> temp(100);  // Allocates ~400 bytes each time
        
        for (int j = 0; j < 100; ++j) {
            temp[j] = i + j;
        }
        
        for (int val : temp) {
            sum += val;
        }
    }  // temp destroyed, memory freed
    
    DoNotOptimize(sum);
}

/*
 * ANTI-PATTERN 2: String concatenation in loop
 * 
 * Each += operation may trigger reallocation and copy of the entire string.
 * For N appends of average length L, this is O(N² * L) complexity!
 */
std::string antipattern_string_concat(int iterations) {
    std::string result;
    for (int i = 0; i < iterations; ++i) {
        // BAD: Each += may reallocate!
        result += "Item number " + std::to_string(i) + " in the list. ";
    }
    return result;
}

/*
 * ANTI-PATTERN 3: Using std::list for sequential access
 * 
 * Each element is a separate heap allocation.
 * N elements = N allocations (plus node overhead).
 */
void antipattern_list_allocations(int count) {
    std::list<int> numbers;  // Linked list
    for (int i = 0; i < count; ++i) {
        // BAD: Each push_back allocates a new node!
        numbers.push_back(i);
    }
    
    long long sum = 0;
    for (int n : numbers) {
        sum += n;
    }
    DoNotOptimize(sum);
}

// ==================== Fixed Patterns ====================

/*
 * FIXED 1: Reuse allocation across iterations
 * 
 * Allocate once, clear and reuse. Only pays allocation cost once.
 */
void fixed_reuse_allocation(int iterations) {
    long long sum = 0;
    
    // GOOD: Allocate once outside the loop
    std::vector<int> temp;
    temp.reserve(100);  // Pre-allocate capacity
    
    for (int i = 0; i < iterations; ++i) {
        temp.clear();  // Clear contents but keep capacity
        
        for (int j = 0; j < 100; ++j) {
            temp.push_back(i + j);  // No allocation if within capacity
        }
        
        for (int val : temp) {
            sum += val;
        }
    }
    
    DoNotOptimize(sum);
}

/*
 * FIXED 2: Use ostringstream or reserve for strings
 * 
 * Pre-calculate size or use a more efficient building method.
 */
std::string fixed_string_build(int iterations) {
    // GOOD: Pre-calculate approximate size
    std::string result;
    result.reserve(iterations * 40);  // Estimate ~40 chars per entry
    
    for (int i = 0; i < iterations; ++i) {
        result += "Item number ";
        result += std::to_string(i);
        result += " in the list. ";
    }
    return result;
}

/*
 * Alternative: Use ostringstream (often faster for complex formatting)
 */
std::string fixed_string_stream(int iterations) {
    std::ostringstream oss;
    for (int i = 0; i < iterations; ++i) {
        oss << "Item number " << i << " in the list. ";
    }
    return oss.str();
}

/*
 * FIXED 3: Use vector instead of list for sequential data
 * 
 * One allocation for all elements (with reserve).
 */
void fixed_vector_allocation(int count) {
    std::vector<int> numbers;
    numbers.reserve(count);  // GOOD: Single allocation
    
    for (int i = 0; i < count; ++i) {
        numbers.push_back(i);  // No allocation (within capacity)
    }
    
    long long sum = 0;
    for (int n : numbers) {
        sum += n;
    }
    DoNotOptimize(sum);
}

// ==================== Benchmark ====================

template<typename Func>
double benchmark(Func f, int runs = 5) {
    // Warm-up
    for (int i = 0; i < 2; ++i) f();
    
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
    std::cout << "=== Allocation Hotspots Identification ===\n\n";
    std::cout << std::fixed << std::setprecision(2);
    
    const int LOOP_ITERATIONS = 100000;
    const int STRING_ITERATIONS = 10000;
    const int LIST_COUNT = 100000;
    
    // ========== Test 1: Loop allocation ==========
    std::cout << "--- Test 1: Vector Allocation in Loop ---\n";
    std::cout << "Iterations: " << LOOP_ITERATIONS << "\n\n";
    
    double anti1_time = benchmark([&]() {
        antipattern_alloc_in_loop(LOOP_ITERATIONS);
    });
    std::cout << "Anti-pattern (alloc each iteration): " << anti1_time << " ms\n";
    
    double fixed1_time = benchmark([&]() {
        fixed_reuse_allocation(LOOP_ITERATIONS);
    });
    std::cout << "Fixed (reuse allocation):            " << fixed1_time << " ms\n";
    std::cout << "Speedup: " << anti1_time / fixed1_time << "x\n\n";
    
    // ========== Test 2: String building ==========
    std::cout << "--- Test 2: String Building ---\n";
    std::cout << "Iterations: " << STRING_ITERATIONS << "\n\n";
    
    std::string result;
    double anti2_time = benchmark([&]() {
        result = antipattern_string_concat(STRING_ITERATIONS);
        DoNotOptimize(result);
    });
    std::cout << "Anti-pattern (naive concat): " << anti2_time << " ms\n";
    std::cout << "  Result length: " << result.length() << " chars\n";
    
    double fixed2a_time = benchmark([&]() {
        result = fixed_string_build(STRING_ITERATIONS);
        DoNotOptimize(result);
    });
    std::cout << "Fixed (reserve):             " << fixed2a_time << " ms\n";
    
    double fixed2b_time = benchmark([&]() {
        result = fixed_string_stream(STRING_ITERATIONS);
        DoNotOptimize(result);
    });
    std::cout << "Fixed (ostringstream):       " << fixed2b_time << " ms\n";
    std::cout << "Speedup (reserve): " << anti2_time / fixed2a_time << "x\n\n";
    
    // ========== Test 3: Container choice ==========
    std::cout << "--- Test 3: Container Choice (list vs vector) ---\n";
    std::cout << "Elements: " << LIST_COUNT << "\n\n";
    
    double anti3_time = benchmark([&]() {
        antipattern_list_allocations(LIST_COUNT);
    });
    std::cout << "std::list (N allocations): " << anti3_time << " ms\n";
    
    double fixed3_time = benchmark([&]() {
        fixed_vector_allocation(LIST_COUNT);
    });
    std::cout << "std::vector (1 allocation): " << fixed3_time << " ms\n";
    std::cout << "Speedup: " << anti3_time / fixed3_time << "x\n\n";
    
    // ========== Profiling Tools Guide ==========
    std::cout << "=== How to Profile Allocations ===\n\n";
    
    std::cout << "1. VALGRIND MASSIF (heap profiler):\n";
    std::cout << "   valgrind --tool=massif ./program\n";
    std::cout << "   ms_print massif.out.<pid>\n\n";
    
    std::cout << "2. HEAPTRACK (allocation tracker):\n";
    std::cout << "   heaptrack ./program\n";
    std::cout << "   heaptrack_gui heaptrack.<program>.<pid>.gz\n\n";
    
    std::cout << "3. PERF (sampling profiler):\n";
    std::cout << "   perf record -g ./program\n";
    std::cout << "   perf report\n";
    std::cout << "   Look for: malloc, free, operator new, operator delete\n\n";
    
    std::cout << "4. SANITIZERS:\n";
    std::cout << "   g++ -fsanitize=address -g program.cpp\n";
    std::cout << "   Shows memory errors and can track allocations\n\n";
    
    std::cout << "=== Key Takeaways ===\n\n";
    std::cout << "1. Allocations in hot loops are major performance killers\n";
    std::cout << "2. Reuse containers with clear() instead of recreating\n";
    std::cout << "3. Use reserve() when you know the size upfront\n";
    std::cout << "4. Prefer vector over list for most use cases\n";
    std::cout << "5. Use profiling tools to find real hotspots\n";
    std::cout << "6. String building: reserve or use ostringstream\n";
    
    return 0;
}
