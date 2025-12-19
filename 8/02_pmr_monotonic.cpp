/**
 * PMR (Polymorphic Memory Resource) and Monotonic Buffer
 * 
 * This code demonstrates how to use C++17's PMR allocators,
 * specifically monotonic_buffer_resource for high-performance
 * allocation patterns.
 * 
 * Key concepts:
 * - PMR provides customizable memory allocation
 * - monotonic_buffer_resource allocates from pre-allocated buffer
 * - No per-object deallocation overhead (bulk release at end)
 * - Perfect for temporary allocations within a scope
 * - Arena-style allocation pattern
 * 
 * Compile: g++ -O3 -std=c++17 -o 02_pmr_monotonic 02_pmr_monotonic.cpp
 * Run: ./02_pmr_monotonic
 */

#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <chrono>
#include <iomanip>
#include <memory_resource>
#include <array>
#include <cstring>

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
volatile size_t sink;

// ============================================
// ALLOCATION PATTERNS
// ============================================

/**
 * Pattern 1: Standard allocator (default new/delete)
 * 
 * Each allocation goes through the heap allocator.
 * Each deallocation returns memory to heap.
 * Can fragment memory over time.
 */
size_t vector_operations_default(int iterations, int elements) {
    size_t total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // Standard vector with default allocator
        std::vector<int> vec;
        vec.reserve(elements);
        
        for (int j = 0; j < elements; ++j) {
            vec.push_back(j * i);
        }
        
        for (int val : vec) {
            total += val;
        }
        // vec deallocated here
    }
    
    return total;
}

/**
 * Pattern 2: PMR with monotonic_buffer_resource
 * 
 * All allocations come from a pre-allocated buffer.
 * No individual deallocations - all freed when resource is destroyed.
 * Very fast allocation (just bump a pointer).
 */
size_t vector_operations_pmr_monotonic(int iterations, int elements) {
    size_t total = 0;
    
    // Pre-allocate buffer for all allocations
    // Size estimation: elements * sizeof(int) * some_overhead
    std::array<std::byte, 1024 * 1024> buffer;  // 1MB buffer
    
    for (int i = 0; i < iterations; ++i) {
        // Create monotonic resource using our buffer
        // When mbr goes out of scope, ALL memory is "freed" instantly
        std::pmr::monotonic_buffer_resource mbr(
            buffer.data(), buffer.size(),
            std::pmr::null_memory_resource()  // No fallback - fail if buffer exhausted
        );
        
        // PMR vector using our resource
        std::pmr::vector<int> vec(&mbr);
        vec.reserve(elements);
        
        for (int j = 0; j < elements; ++j) {
            vec.push_back(j * i);
        }
        
        for (int val : vec) {
            total += val;
        }
        // vec destroyed, but monotonic doesn't deallocate individual elements
        // mbr destroyed - all memory "freed" at once (actually just abandoned)
    }
    
    return total;
}

/**
 * Pattern 3: PMR with persistent buffer across iterations
 * 
 * Reuse the same buffer for each iteration.
 * Reset the monotonic resource instead of recreating.
 */
size_t vector_operations_pmr_reuse(int iterations, int elements) {
    size_t total = 0;
    
    // Allocate buffer once for all iterations
    std::array<std::byte, 1024 * 1024> buffer;
    
    // Create resource once - we'll reset it each iteration
    std::pmr::monotonic_buffer_resource mbr(
        buffer.data(), buffer.size(),
        std::pmr::null_memory_resource()
    );
    
    for (int i = 0; i < iterations; ++i) {
        {
            std::pmr::vector<int> vec(&mbr);
            vec.reserve(elements);
            
            for (int j = 0; j < elements; ++j) {
                vec.push_back(j * i);
            }
            
            for (int val : vec) {
                total += val;
            }
        }
        
        // Reset the resource - "frees" all allocations by resetting pointer
        // Note: monotonic_buffer_resource doesn't have reset(), 
        // but we can achieve similar effect by recreating it
        mbr.release();  // Release any upstream allocations
        // Recreate with same buffer
        new (&mbr) std::pmr::monotonic_buffer_resource(
            buffer.data(), buffer.size(),
            std::pmr::null_memory_resource()
        );
    }
    
    return total;
}

// ============================================
// LINKED LIST EXAMPLE
// ============================================

/**
 * std::list with default allocator
 * 
 * Each node allocation goes to heap.
 * Each node deallocation returns to heap.
 * Nodes scattered in memory - poor cache locality.
 */
size_t list_operations_default(int iterations, int elements) {
    size_t total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        std::list<int> lst;
        
        for (int j = 0; j < elements; ++j) {
            lst.push_back(j * i);  // Each push_back allocates a node
        }
        
        for (int val : lst) {
            total += val;
        }
        // All nodes deallocated one by one
    }
    
    return total;
}

/**
 * PMR list with monotonic allocator
 * 
 * All nodes allocated from contiguous buffer.
 * Better cache locality.
 * No individual deallocation overhead.
 */
size_t list_operations_pmr(int iterations, int elements) {
    size_t total = 0;
    
    // Buffer needs to hold all list nodes
    // Node size = sizeof(int) + 2 * sizeof(pointer) + alignment
    std::array<std::byte, 2 * 1024 * 1024> buffer;  // 2MB
    
    for (int i = 0; i < iterations; ++i) {
        std::pmr::monotonic_buffer_resource mbr(
            buffer.data(), buffer.size(),
            std::pmr::null_memory_resource()
        );
        
        std::pmr::list<int> lst(&mbr);
        
        for (int j = 0; j < elements; ++j) {
            lst.push_back(j * i);  // Allocates from our buffer
        }
        
        for (int val : lst) {
            total += val;
        }
        // No individual node deallocation!
    }
    
    return total;
}

// ============================================
// MAP EXAMPLE
// ============================================

/**
 * std::map with default allocator
 */
size_t map_operations_default(int iterations, int elements) {
    size_t total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        std::map<int, int> m;
        
        for (int j = 0; j < elements; ++j) {
            m[j] = j * i;  // Tree node allocation
        }
        
        for (const auto& [key, val] : m) {
            total += val;
        }
    }
    
    return total;
}

/**
 * PMR map with monotonic allocator
 */
size_t map_operations_pmr(int iterations, int elements) {
    size_t total = 0;
    
    std::array<std::byte, 4 * 1024 * 1024> buffer;  // 4MB for tree nodes
    
    for (int i = 0; i < iterations; ++i) {
        std::pmr::monotonic_buffer_resource mbr(
            buffer.data(), buffer.size(),
            std::pmr::null_memory_resource()
        );
        
        std::pmr::map<int, int> m(&mbr);
        
        for (int j = 0; j < elements; ++j) {
            m[j] = j * i;
        }
        
        for (const auto& [key, val] : m) {
            total += val;
        }
    }
    
    return total;
}

// ============================================
// STRING EXAMPLE
// ============================================

/**
 * String operations with default allocator
 */
size_t string_operations_default(int iterations) {
    size_t total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::string> strings;
        strings.reserve(100);
        
        for (int j = 0; j < 100; ++j) {
            // Each string may allocate (if > SSO threshold)
            strings.push_back("This is a somewhat long string that exceeds SSO " + std::to_string(j));
        }
        
        for (const auto& s : strings) {
            total += s.length();
        }
    }
    
    return total;
}

/**
 * PMR string operations
 */
size_t string_operations_pmr(int iterations) {
    size_t total = 0;
    
    std::array<std::byte, 1024 * 1024> buffer;
    
    for (int i = 0; i < iterations; ++i) {
        std::pmr::monotonic_buffer_resource mbr(
            buffer.data(), buffer.size(),
            std::pmr::null_memory_resource()
        );
        
        std::pmr::vector<std::pmr::string> strings(&mbr);
        strings.reserve(100);
        
        for (int j = 0; j < 100; ++j) {
            // PMR string uses the monotonic allocator
            std::pmr::string s(&mbr);
            s = "This is a somewhat long string that exceeds SSO ";
            s += std::to_string(j);
            strings.push_back(std::move(s));
        }
        
        for (const auto& s : strings) {
            total += s.length();
        }
    }
    
    return total;
}

int main() {
    std::cout << "=== PMR Monotonic Buffer Demo ===\n\n";
    
    // Configuration
    const int iterations = 1000;
    const int elements = 10000;
    
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Elements per iteration: " << elements << "\n\n";
    
    // ==========================================
    // Test 1: Vector operations
    // ==========================================
    std::cout << "=== Test 1: std::vector Operations ===\n";
    
    {
        Timer timer;
        sink = vector_operations_default(iterations, elements);
        std::cout << "Default allocator: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = vector_operations_pmr_monotonic(iterations, elements);
        std::cout << "PMR monotonic: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = vector_operations_pmr_reuse(iterations, elements);
        std::cout << "PMR with buffer reuse: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Test 2: List operations (shows bigger difference)
    // ==========================================
    std::cout << "=== Test 2: std::list Operations ===\n";
    std::cout << "(Many small allocations - PMR shines here)\n";
    
    {
        Timer timer;
        sink = list_operations_default(iterations, elements);
        std::cout << "Default allocator: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = list_operations_pmr(iterations, elements);
        std::cout << "PMR monotonic: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Test 3: Map operations
    // ==========================================
    std::cout << "=== Test 3: std::map Operations ===\n";
    std::cout << "(Tree node allocations)\n";
    
    {
        Timer timer;
        sink = map_operations_default(iterations / 10, elements);
        std::cout << "Default allocator: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = map_operations_pmr(iterations / 10, elements);
        std::cout << "PMR monotonic: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Test 4: String operations
    // ==========================================
    std::cout << "=== Test 4: String Operations ===\n";
    
    {
        Timer timer;
        sink = string_operations_default(iterations);
        std::cout << "Default allocator: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = string_operations_pmr(iterations);
        std::cout << "PMR monotonic: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    std::cout << "\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "How monotonic_buffer_resource works:\n\n";
    
    std::cout << "Standard allocator:       Monotonic allocator:\n";
    std::cout << "+-----------------+       +-----------------+\n";
    std::cout << "| Heap            |       | Pre-allocated   |\n";
    std::cout << "| +---+ +---+     |       | Buffer          |\n";
    std::cout << "| | A | | B | ... |       | [A][B][C][D]... |\n";
    std::cout << "| +---+ +---+     |       |  ^              |\n";
    std::cout << "| (scattered)     |       |  ptr (bumps up) |\n";
    std::cout << "+-----------------+       +-----------------+\n\n";
    
    std::cout << "Benefits of PMR monotonic:\n\n";
    
    std::cout << "1. FAST ALLOCATION:\n";
    std::cout << "   - Just increment a pointer\n";
    std::cout << "   - No free-list management\n";
    std::cout << "   - No lock contention\n\n";
    
    std::cout << "2. FAST DEALLOCATION:\n";
    std::cout << "   - Individual dealloc is no-op\n";
    std::cout << "   - Bulk release when resource destroyed\n";
    std::cout << "   - O(1) regardless of allocation count\n\n";
    
    std::cout << "3. BETTER LOCALITY:\n";
    std::cout << "   - Allocations are contiguous\n";
    std::cout << "   - Better cache behavior\n";
    std::cout << "   - No fragmentation\n\n";
    
    std::cout << "When to use:\n\n";
    
    std::cout << "- Temporary allocations within a scope\n";
    std::cout << "- Request handling (allocate, process, free all)\n";
    std::cout << "- Frame-based allocation (games, simulations)\n";
    std::cout << "- When allocation pattern is allocate-many-then-free-all\n\n";
    
    std::cout << "PMR containers:\n";
    std::cout << "- std::pmr::vector<T>\n";
    std::cout << "- std::pmr::list<T>\n";
    std::cout << "- std::pmr::map<K,V>\n";
    std::cout << "- std::pmr::string\n";
    std::cout << "- std::pmr::deque<T>\n";
    std::cout << "- etc.\n";
    
    return 0;
}
