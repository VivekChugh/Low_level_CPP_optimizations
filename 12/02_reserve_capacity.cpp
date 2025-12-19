/*
 * 02_reserve_capacity.cpp
 * 
 * CONCEPT: Good Pattern: Reserve Capacity
 * TECHNIQUE: Compiler Assistance / Memory Efficiency
 * 
 * This program demonstrates the importance of using reserve() to
 * pre-allocate capacity in containers like std::vector and std::string.
 * 
 * Without reserve(), containers grow dynamically:
 *   1. Start with small capacity
 *   2. When full, allocate larger buffer (typically 1.5x or 2x)
 *   3. Copy all elements to new buffer
 *   4. Delete old buffer
 * 
 * For N insertions, this causes O(log N) reallocations and O(N) total copies.
 * With reserve(N), there's exactly 1 allocation and 0 copies.
 * 
 * COMPILATION:
 *   g++ -O3 -o 02_reserve_capacity 02_reserve_capacity.cpp
 * 
 * RUN:
 *   ./02_reserve_capacity
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>

/*
 * DoNotOptimize: Prevent dead code elimination
 */
template<typename T>
void DoNotOptimize(T& value) {
    asm volatile("" : "+r"(value) : : "memory");
}

/*
 * AllocationCounter: Track vector reallocations
 * 
 * We can't easily count allocations, but we can track when
 * capacity changes (which indicates a reallocation).
 */
template<typename T>
class VectorWithTracking {
public:
    std::vector<T> vec;
    int reallocation_count = 0;
    size_t last_capacity = 0;
    
    void push_back(const T& value) {
        if (vec.capacity() != last_capacity) {
            ++reallocation_count;
            last_capacity = vec.capacity();
        }
        vec.push_back(value);
        if (vec.capacity() != last_capacity) {
            ++reallocation_count;
            last_capacity = vec.capacity();
        }
    }
    
    void reserve(size_t n) {
        vec.reserve(n);
        if (vec.capacity() != last_capacity) {
            ++reallocation_count;
            last_capacity = vec.capacity();
        }
    }
    
    void clear() {
        vec.clear();
        // Note: clear() doesn't reduce capacity
    }
    
    size_t size() const { return vec.size(); }
    size_t capacity() const { return vec.capacity(); }
};

// ==================== Without Reserve ====================

/*
 * NO RESERVE: Vector grows dynamically
 * 
 * For N elements, approximately log2(N) reallocations occur.
 * Each reallocation copies all existing elements.
 */
std::vector<int> build_vector_no_reserve(int n) {
    std::vector<int> result;  // Starts empty
    
    // No reserve - will reallocate multiple times
    for (int i = 0; i < n; ++i) {
        result.push_back(i);
    }
    
    return result;
}

/*
 * Track reallocations for demonstration
 */
void demonstrate_reallocations(int n) {
    VectorWithTracking<int> tracked;
    
    std::cout << "Building vector of " << n << " elements without reserve:\n";
    std::cout << "Size -> Capacity (after push_back)\n";
    
    size_t last_printed_cap = 0;
    for (int i = 0; i < n; ++i) {
        tracked.push_back(i);
        
        // Print when capacity changes (reallocation occurred)
        if (tracked.capacity() != last_printed_cap && tracked.size() <= 100) {
            std::cout << "  " << tracked.size() << " -> " << tracked.capacity() << "\n";
            last_printed_cap = tracked.capacity();
        }
    }
    
    std::cout << "\nTotal reallocations: " << tracked.reallocation_count << "\n";
    std::cout << "Final size: " << tracked.size() << ", Final capacity: " << tracked.capacity() << "\n\n";
}

// ==================== With Reserve ====================

/*
 * WITH RESERVE: Single allocation upfront
 * 
 * One allocation of exact size needed.
 * All push_backs are O(1) without any copying.
 */
std::vector<int> build_vector_with_reserve(int n) {
    std::vector<int> result;
    result.reserve(n);  // Single allocation
    
    for (int i = 0; i < n; ++i) {
        result.push_back(i);  // No reallocation
    }
    
    return result;
}

/*
 * Track with reserve
 */
void demonstrate_with_reserve(int n) {
    VectorWithTracking<int> tracked;
    tracked.reserve(n);  // Pre-allocate
    
    std::cout << "Building vector of " << n << " elements WITH reserve:\n";
    
    for (int i = 0; i < n; ++i) {
        tracked.push_back(i);
    }
    
    std::cout << "Total reallocations: " << tracked.reallocation_count << "\n";
    std::cout << "Final size: " << tracked.size() << ", Final capacity: " << tracked.capacity() << "\n\n";
}

// ==================== String Reserve ====================

/*
 * String without reserve
 */
std::string build_string_no_reserve(int n) {
    std::string result;
    
    for (int i = 0; i < n; ++i) {
        result += "X";  // May reallocate multiple times
    }
    
    return result;
}

/*
 * String with reserve
 */
std::string build_string_with_reserve(int n) {
    std::string result;
    result.reserve(n);  // Pre-allocate
    
    for (int i = 0; i < n; ++i) {
        result += "X";  // No reallocation
    }
    
    return result;
}

// ==================== Benchmark ====================

template<typename Func>
double benchmark(Func f, int runs = 10) {
    // Warm-up
    for (int i = 0; i < 3; ++i) {
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
    std::cout << "=== Reserve Capacity Pattern ===\n\n";
    std::cout << std::fixed << std::setprecision(3);
    
    // ========== Demonstrate Reallocations ==========
    std::cout << "--- Visualizing Reallocations ---\n\n";
    demonstrate_reallocations(1000);
    demonstrate_with_reserve(1000);
    
    // ========== Benchmark Vector ==========
    std::cout << "--- Benchmark: Vector Building ---\n\n";
    
    std::vector<int> test_sizes = {10000, 100000, 1000000};
    
    for (int n : test_sizes) {
        std::cout << "N = " << n << ":\n";
        
        double no_reserve_time = benchmark([n]() {
            return build_vector_no_reserve(n);
        });
        
        double with_reserve_time = benchmark([n]() {
            return build_vector_with_reserve(n);
        });
        
        std::cout << "  Without reserve: " << no_reserve_time << " ms\n";
        std::cout << "  With reserve:    " << with_reserve_time << " ms\n";
        std::cout << "  Speedup:         " << no_reserve_time / with_reserve_time << "x\n\n";
    }
    
    // ========== Benchmark String ==========
    std::cout << "--- Benchmark: String Building ---\n\n";
    
    for (int n : test_sizes) {
        std::cout << "N = " << n << ":\n";
        
        double no_reserve_time = benchmark([n]() {
            return build_string_no_reserve(n);
        });
        
        double with_reserve_time = benchmark([n]() {
            return build_string_with_reserve(n);
        });
        
        std::cout << "  Without reserve: " << no_reserve_time << " ms\n";
        std::cout << "  With reserve:    " << with_reserve_time << " ms\n";
        std::cout << "  Speedup:         " << no_reserve_time / with_reserve_time << "x\n\n";
    }
    
    // ========== Memory Analysis ==========
    std::cout << "--- Memory Analysis ---\n\n";
    
    std::vector<int> v1, v2;
    
    // Without reserve
    for (int i = 0; i < 1000; ++i) v1.push_back(i);
    std::cout << "Without reserve (1000 elements):\n";
    std::cout << "  Size: " << v1.size() << ", Capacity: " << v1.capacity() << "\n";
    std::cout << "  Wasted space: " << (v1.capacity() - v1.size()) * sizeof(int) << " bytes\n\n";
    
    // With reserve
    v2.reserve(1000);
    for (int i = 0; i < 1000; ++i) v2.push_back(i);
    std::cout << "With reserve (1000 elements):\n";
    std::cout << "  Size: " << v2.size() << ", Capacity: " << v2.capacity() << "\n";
    std::cout << "  Wasted space: " << (v2.capacity() - v2.size()) * sizeof(int) << " bytes\n\n";
    
    // ========== Additional Patterns ==========
    std::cout << "=== Related Patterns ===\n\n";
    
    std::cout << "1. SHRINK TO FIT:\n";
    std::cout << "   vector.shrink_to_fit() - Reduce capacity to match size\n";
    std::cout << "   Useful after removing many elements\n\n";
    
    std::cout << "2. RESIZE vs RESERVE:\n";
    std::cout << "   reserve(n) - Allocates capacity, size unchanged\n";
    std::cout << "   resize(n)  - Changes size, default-constructs elements\n\n";
    
    std::cout << "3. CLEAR vs SHRINK:\n";
    std::cout << "   clear()          - Removes elements, keeps capacity\n";
    std::cout << "   shrink_to_fit()  - Requests capacity reduction\n";
    std::cout << "   swap trick       - vector<T>().swap(v) forces capacity to 0\n\n";
    
    std::cout << "4. EMPLACE vs PUSH:\n";
    std::cout << "   emplace_back() - Constructs in-place, may avoid copy\n";
    std::cout << "   push_back()    - Copies/moves existing object\n\n";
    
    // ========== Summary ==========
    std::cout << "=== Key Takeaways ===\n\n";
    
    std::cout << "1. ALWAYS use reserve() when you know the final size\n";
    std::cout << "2. Even approximate sizes help (reserve(n/2) better than nothing)\n";
    std::cout << "3. Benefits:\n";
    std::cout << "   - Fewer allocations (1 vs log N)\n";
    std::cout << "   - No element copying during growth\n";
    std::cout << "   - Better cache locality\n";
    std::cout << "   - Predictable memory usage\n\n";
    
    std::cout << "4. Applies to: vector, string, unordered_map, unordered_set\n";
    std::cout << "   (map/set don't have reserve - consider unordered variants)\n";
    
    return 0;
}
