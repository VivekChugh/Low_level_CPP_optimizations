/**
 * unique_ptr vs shared_ptr Performance Comparison
 * 
 * This code demonstrates the performance difference between
 * unique_ptr and shared_ptr, and when to use each.
 * 
 * Key concepts:
 * - unique_ptr has zero overhead compared to raw pointer
 * - shared_ptr has reference counting overhead
 * - Reference counting: atomic operations (expensive!)
 * - shared_ptr control block: extra allocation
 * - make_shared: single allocation optimization
 * 
 * Compile: g++ -O3 -std=c++17 -o 03_unique_vs_shared 03_unique_vs_shared.cpp
 * Run: ./03_unique_vs_shared
 */

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <iomanip>
#include <atomic>

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
volatile int sink;

// ============================================
// SIMPLE OBJECT FOR TESTING
// ============================================

class Widget {
    int data[10];  // Some data
public:
    Widget() {
        for (int i = 0; i < 10; ++i) data[i] = i;
    }
    int value() const { return data[0]; }
    void setValue(int v) { data[0] = v; }
};

// ============================================
// SIZE COMPARISON
// ============================================

void show_sizes() {
    std::cout << "=== Size Comparison ===\n";
    std::cout << "sizeof(Widget*):            " << sizeof(Widget*) << " bytes\n";
    std::cout << "sizeof(unique_ptr<Widget>): " << sizeof(std::unique_ptr<Widget>) << " bytes\n";
    std::cout << "sizeof(shared_ptr<Widget>): " << sizeof(std::shared_ptr<Widget>) << " bytes\n";
    std::cout << "sizeof(weak_ptr<Widget>):   " << sizeof(std::weak_ptr<Widget>) << " bytes\n";
    std::cout << "\nNote: shared_ptr is 2 pointers (object + control block)\n\n";
}

// ============================================
// CREATION TESTS
// ============================================

/**
 * Raw pointer creation
 */
double test_raw_creation(int iterations) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        Widget* w = new Widget();
        sink = w->value();
        delete w;
    }
    
    return timer.elapsed_ms();
}

/**
 * unique_ptr creation with new
 */
double test_unique_ptr_new(int iterations) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        // One allocation for Widget
        std::unique_ptr<Widget> w(new Widget());
        sink = w->value();
        // Automatic deletion
    }
    
    return timer.elapsed_ms();
}

/**
 * unique_ptr creation with make_unique
 * 
 * make_unique is preferred for exception safety,
 * but performance is same as new.
 */
double test_unique_ptr_make(int iterations) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        // One allocation for Widget
        auto w = std::make_unique<Widget>();
        sink = w->value();
    }
    
    return timer.elapsed_ms();
}

/**
 * shared_ptr creation with new
 * 
 * TWO allocations:
 * 1. Widget object
 * 2. Control block (reference counts, deleter, etc.)
 */
double test_shared_ptr_new(int iterations) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        // Two allocations!
        std::shared_ptr<Widget> w(new Widget());
        sink = w->value();
        // Reference count decremented, Widget deleted
    }
    
    return timer.elapsed_ms();
}

/**
 * shared_ptr creation with make_shared
 * 
 * SINGLE allocation for both Widget and control block.
 * This is more cache-friendly and faster.
 */
double test_shared_ptr_make(int iterations) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        // Single allocation for Widget + control block
        auto w = std::make_shared<Widget>();
        sink = w->value();
    }
    
    return timer.elapsed_ms();
}

// ============================================
// COPY TESTS (where shared_ptr shines and hurts)
// ============================================

/**
 * Passing unique_ptr by reference (no copy possible)
 */
void process_unique_ref(const std::unique_ptr<Widget>& w) {
    sink = w->value();
}

double test_unique_ptr_pass_ref(int iterations) {
    auto w = std::make_unique<Widget>();
    
    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        process_unique_ref(w);  // No copy, just reference
    }
    return timer.elapsed_ms();
}

/**
 * Passing shared_ptr by value (COPIES - expensive!)
 * 
 * Each copy:
 * - Atomically increments reference count
 * - Atomic operations can cost 10-100 cycles
 */
void process_shared_value(std::shared_ptr<Widget> w) {
    sink = w->value();
    // Reference count decremented on exit
}

double test_shared_ptr_pass_value(int iterations) {
    auto w = std::make_shared<Widget>();
    
    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        process_shared_value(w);  // Copies shared_ptr!
    }
    return timer.elapsed_ms();
}

/**
 * Passing shared_ptr by const reference (NO copy)
 * 
 * This is the efficient way to pass shared_ptr.
 */
void process_shared_ref(const std::shared_ptr<Widget>& w) {
    sink = w->value();
}

double test_shared_ptr_pass_ref(int iterations) {
    auto w = std::make_shared<Widget>();
    
    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        process_shared_ref(w);  // No copy!
    }
    return timer.elapsed_ms();
}

/**
 * Passing raw pointer (baseline)
 */
void process_raw(Widget* w) {
    sink = w->value();
}

double test_raw_ptr_pass(int iterations) {
    auto w = std::make_unique<Widget>();
    
    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        process_raw(w.get());
    }
    return timer.elapsed_ms();
}

// ============================================
// CONTAINER TESTS
// ============================================

/**
 * Vector of unique_ptr
 * 
 * - Elements are move-only
 * - No reference counting overhead
 * - Objects scattered in memory (each unique_ptr points elsewhere)
 */
double test_vector_unique(int iterations, int size) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::unique_ptr<Widget>> vec;
        vec.reserve(size);
        
        for (int j = 0; j < size; ++j) {
            vec.push_back(std::make_unique<Widget>());
        }
        
        int sum = 0;
        for (const auto& w : vec) {
            sum += w->value();
        }
        sink = sum;
    }
    
    return timer.elapsed_ms();
}

/**
 * Vector of shared_ptr
 * 
 * - Can copy elements
 * - Reference counting on copy
 */
double test_vector_shared(int iterations, int size) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        std::vector<std::shared_ptr<Widget>> vec;
        vec.reserve(size);
        
        for (int j = 0; j < size; ++j) {
            vec.push_back(std::make_shared<Widget>());
        }
        
        int sum = 0;
        for (const auto& w : vec) {
            sum += w->value();
        }
        sink = sum;
    }
    
    return timer.elapsed_ms();
}

/**
 * Vector of objects (best for cache)
 * 
 * - Objects stored contiguously
 * - Best cache locality
 * - No pointer chasing
 */
double test_vector_objects(int iterations, int size) {
    Timer timer;
    
    for (int i = 0; i < iterations; ++i) {
        std::vector<Widget> vec;
        vec.reserve(size);
        
        for (int j = 0; j < size; ++j) {
            vec.emplace_back();
        }
        
        int sum = 0;
        for (const auto& w : vec) {
            sum += w.value();
        }
        sink = sum;
    }
    
    return timer.elapsed_ms();
}

// ============================================
// REFERENCE COUNT CONTENTION
// ============================================

double test_shared_ptr_contention_single(int iterations) {
    auto w = std::make_shared<Widget>();
    
    Timer timer;
    for (int i = 0; i < iterations; ++i) {
        // Each copy/destroy touches the atomic ref count
        auto copy = w;  // Atomic increment
        sink = copy->value();
        // Atomic decrement on destruction
    }
    return timer.elapsed_ms();
}

int main() {
    std::cout << "=== unique_ptr vs shared_ptr ===\n\n";
    
    show_sizes();
    
    const int iterations = 1000000;
    const int container_size = 1000;
    const int container_iters = 1000;
    
    std::cout << "Iterations: " << iterations << "\n\n";
    
    // ==========================================
    // Creation tests
    // ==========================================
    std::cout << "=== Creation Tests ===\n";
    
    std::cout << "Raw new/delete: " << std::fixed << std::setprecision(2)
              << test_raw_creation(iterations) << " ms\n";
    
    std::cout << "unique_ptr(new): " << std::fixed << std::setprecision(2)
              << test_unique_ptr_new(iterations) << " ms\n";
    
    std::cout << "make_unique: " << std::fixed << std::setprecision(2)
              << test_unique_ptr_make(iterations) << " ms\n";
    
    std::cout << "shared_ptr(new): " << std::fixed << std::setprecision(2)
              << test_shared_ptr_new(iterations) << " ms  <-- 2 allocations!\n";
    
    std::cout << "make_shared: " << std::fixed << std::setprecision(2)
              << test_shared_ptr_make(iterations) << " ms  <-- 1 allocation\n";
    
    std::cout << "\n";
    
    // ==========================================
    // Pass by value/reference tests
    // ==========================================
    std::cout << "=== Pass to Function Tests ===\n";
    
    std::cout << "Raw pointer: " << std::fixed << std::setprecision(2)
              << test_raw_ptr_pass(iterations) << " ms\n";
    
    std::cout << "unique_ptr const&: " << std::fixed << std::setprecision(2)
              << test_unique_ptr_pass_ref(iterations) << " ms\n";
    
    std::cout << "shared_ptr const&: " << std::fixed << std::setprecision(2)
              << test_shared_ptr_pass_ref(iterations) << " ms\n";
    
    std::cout << "shared_ptr by VALUE: " << std::fixed << std::setprecision(2)
              << test_shared_ptr_pass_value(iterations) << " ms  <-- Atomic ops!\n";
    
    std::cout << "\n";
    
    // ==========================================
    // Container tests
    // ==========================================
    std::cout << "=== Container Tests (size=" << container_size << ") ===\n";
    
    std::cout << "vector<Widget>: " << std::fixed << std::setprecision(2)
              << test_vector_objects(container_iters, container_size) << " ms  <-- Best locality\n";
    
    std::cout << "vector<unique_ptr>: " << std::fixed << std::setprecision(2)
              << test_vector_unique(container_iters, container_size) << " ms\n";
    
    std::cout << "vector<shared_ptr>: " << std::fixed << std::setprecision(2)
              << test_vector_shared(container_iters, container_size) << " ms\n";
    
    std::cout << "\n";
    
    // ==========================================
    // Ref count contention
    // ==========================================
    std::cout << "=== Reference Count Overhead ===\n";
    
    std::cout << "shared_ptr copy/destroy: " << std::fixed << std::setprecision(2)
              << test_shared_ptr_contention_single(iterations) << " ms\n";
    
    std::cout << "\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "unique_ptr:\n";
    std::cout << "  - Zero overhead vs raw pointer\n";
    std::cout << "  - Move-only (no copy)\n";
    std::cout << "  - Single allocation\n";
    std::cout << "  - Use for: Exclusive ownership\n\n";
    
    std::cout << "shared_ptr:\n";
    std::cout << "  - 2x pointer size\n";
    std::cout << "  - Reference counting (atomic operations)\n";
    std::cout << "  - Extra allocation for control block (unless make_shared)\n";
    std::cout << "  - Use for: Shared ownership, when needed\n\n";
    
    std::cout << "Best practices:\n\n";
    
    std::cout << "1. PREFER unique_ptr:\n";
    std::cout << "   - Default choice for owned pointers\n";
    std::cout << "   - Zero overhead\n";
    std::cout << "   - Clear ownership semantics\n\n";
    
    std::cout << "2. USE shared_ptr ONLY WHEN NEEDED:\n";
    std::cout << "   - Multiple owners of same object\n";
    std::cout << "   - Unknown lifetime dependencies\n";
    std::cout << "   - Caching scenarios\n\n";
    
    std::cout << "3. PREFER make_shared over shared_ptr(new):\n";
    std::cout << "   - Single allocation\n";
    std::cout << "   - Better cache locality\n";
    std::cout << "   - Exception safe\n\n";
    
    std::cout << "4. PASS BY REFERENCE:\n";
    std::cout << "   void process(const shared_ptr<T>& p);  // Good\n";
    std::cout << "   void process(shared_ptr<T> p);         // Atomic ops!\n\n";
    
    std::cout << "5. CONSIDER OBJECTS IN CONTAINERS:\n";
    std::cout << "   vector<Widget>         // Best cache locality\n";
    std::cout << "   vector<unique_ptr<W>>  // When polymorphism needed\n";
    std::cout << "   vector<shared_ptr<W>>  // When sharing needed\n";
    
    return 0;
}
