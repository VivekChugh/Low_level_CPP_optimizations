/**
 * Avoiding Allocations in Hot Loops
 * 
 * This code demonstrates the performance impact of memory allocations
 * inside hot loops and techniques to avoid them.
 * 
 * Key concepts:
 * - malloc/new are expensive (system calls, lock contention)
 * - Reuse buffers instead of reallocating
 * - Pre-allocate with reserve()
 * - Move allocations outside loops
 * - Use object pools for frequent alloc/dealloc
 * 
 * Compile: g++ -O3 -std=c++17 -o 01_hot_loop_allocation 01_hot_loop_allocation.cpp
 * Run: ./01_hot_loop_allocation
 */

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>
#include <memory>
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
// ALLOCATION COUNTING
// ============================================

static size_t allocation_count = 0;
static size_t deallocation_count = 0;
static size_t total_bytes_allocated = 0;

void reset_allocation_stats() {
    allocation_count = 0;
    deallocation_count = 0;
    total_bytes_allocated = 0;
}

void print_allocation_stats(const std::string& label) {
    std::cout << label << ":\n";
    std::cout << "  Allocations: " << allocation_count << "\n";
    std::cout << "  Deallocations: " << deallocation_count << "\n";
    std::cout << "  Total bytes: " << total_bytes_allocated << "\n\n";
}

// Custom allocator that counts allocations (for demonstration)
// Note: In production, use profiling tools instead

// ============================================
// EXAMPLE 1: VECTOR GROWTH IN LOOP
// ============================================

/**
 * BAD: Create new vector in every iteration
 * 
 * Each iteration:
 * - Allocates memory for vector
 * - Deallocates at end of iteration
 * - Terrible for cache locality
 */
size_t process_bad_allocation(int iterations, int elements_per_iter) {
    size_t total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // NEW ALLOCATION EVERY ITERATION!
        std::vector<int> temp(elements_per_iter);
        
        // Do some work
        for (int j = 0; j < elements_per_iter; ++j) {
            temp[j] = i * j;
        }
        
        // Process results
        for (int val : temp) {
            total += val;
        }
        
        // temp destroyed here - DEALLOCATION
    }
    
    return total;
}

/**
 * GOOD: Reuse vector across iterations
 * 
 * Allocate once outside loop, clear and reuse.
 * clear() doesn't deallocate - just resets size to 0.
 */
size_t process_reuse_buffer(int iterations, int elements_per_iter) {
    size_t total = 0;
    
    // Allocate ONCE outside the loop
    std::vector<int> temp;
    temp.reserve(elements_per_iter);  // Pre-allocate capacity
    
    for (int i = 0; i < iterations; ++i) {
        // clear() keeps capacity, just resets size to 0
        temp.clear();
        
        // Fill vector (no reallocation needed)
        for (int j = 0; j < elements_per_iter; ++j) {
            temp.push_back(i * j);
        }
        
        // Process results
        for (int val : temp) {
            total += val;
        }
        
        // temp NOT destroyed - reused next iteration
    }
    
    return total;
}

/**
 * BETTER: Use resize instead of push_back
 * 
 * resize() to exact size, then use indexing.
 * Avoids push_back overhead.
 */
size_t process_resize_pattern(int iterations, int elements_per_iter) {
    size_t total = 0;
    
    // Allocate with exact size
    std::vector<int> temp(elements_per_iter);
    
    for (int i = 0; i < iterations; ++i) {
        // Direct indexing - no size checks
        for (int j = 0; j < elements_per_iter; ++j) {
            temp[j] = i * j;
        }
        
        for (int val : temp) {
            total += val;
        }
    }
    
    return total;
}

// ============================================
// EXAMPLE 2: STRING CONCATENATION
// ============================================

/**
 * BAD: String concatenation creates many temporaries
 * 
 * Each + creates a new string with allocation.
 */
std::string build_string_bad(int parts) {
    std::string result;
    
    for (int i = 0; i < parts; ++i) {
        // Each concatenation may allocate!
        result = result + "part" + std::to_string(i) + ",";
    }
    
    return result;
}

/**
 * GOOD: Use reserve and append
 * 
 * Pre-calculate size, reserve capacity, then append.
 */
std::string build_string_good(int parts) {
    std::string result;
    
    // Estimate and reserve capacity
    result.reserve(parts * 10);  // Rough estimate
    
    for (int i = 0; i < parts; ++i) {
        // append() doesn't create temporaries
        result.append("part");
        result.append(std::to_string(i));
        result.append(",");
    }
    
    return result;
}

/**
 * BEST: Use ostringstream or fmt library
 * (ostringstream shown here)
 */
#include <sstream>
std::string build_string_stream(int parts) {
    std::ostringstream oss;
    
    for (int i = 0; i < parts; ++i) {
        oss << "part" << i << ",";
    }
    
    return oss.str();
}

// ============================================
// EXAMPLE 3: OBJECT CREATION IN LOOP
// ============================================

/**
 * A class that does heap allocation in constructor
 */
class HeavyObject {
    std::unique_ptr<int[]> data;
    size_t size;
public:
    explicit HeavyObject(size_t sz) : data(new int[sz]), size(sz) {
        std::memset(data.get(), 0, sz * sizeof(int));
    }
    
    void reset() {
        std::memset(data.get(), 0, size * sizeof(int));
    }
    
    void process(int value) {
        for (size_t i = 0; i < size; ++i) {
            data[i] = value * (i + 1);
        }
    }
    
    int sum() const {
        int total = 0;
        for (size_t i = 0; i < size; ++i) {
            total += data[i];
        }
        return total;
    }
};

/**
 * BAD: Create object in every iteration
 */
size_t process_objects_bad(int iterations, size_t object_size) {
    size_t total = 0;
    
    for (int i = 0; i < iterations; ++i) {
        // Allocation in every iteration!
        HeavyObject obj(object_size);
        obj.process(i);
        total += obj.sum();
        // Deallocation here
    }
    
    return total;
}

/**
 * GOOD: Reuse object across iterations
 */
size_t process_objects_good(int iterations, size_t object_size) {
    size_t total = 0;
    
    // Create once
    HeavyObject obj(object_size);
    
    for (int i = 0; i < iterations; ++i) {
        obj.reset();  // Reset state
        obj.process(i);
        total += obj.sum();
    }
    
    return total;
}

// ============================================
// EXAMPLE 4: TEMPORARY ALLOCATIONS
// ============================================

/**
 * BAD: Function creates temporary buffer each call
 */
std::vector<int> transform_bad(const std::vector<int>& input) {
    std::vector<int> result(input.size());  // Allocation
    
    for (size_t i = 0; i < input.size(); ++i) {
        result[i] = input[i] * 2;
    }
    
    return result;  // Another allocation for copy (maybe RVO helps)
}

/**
 * GOOD: Accept output buffer as parameter
 */
void transform_good(const std::vector<int>& input, std::vector<int>& output) {
    output.resize(input.size());  // May not allocate if capacity sufficient
    
    for (size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] * 2;
    }
}

/**
 * BEST: In-place transformation
 */
void transform_inplace(std::vector<int>& data) {
    for (int& val : data) {
        val *= 2;
    }
}

int main() {
    std::cout << "=== Hot Loop Allocation Demo ===\n\n";
    
    // Configuration
    const int iterations = 10000;
    const int elements = 1000;
    
    std::cout << "Iterations: " << iterations << "\n";
    std::cout << "Elements per iteration: " << elements << "\n\n";
    
    // ==========================================
    // Test 1: Vector allocation patterns
    // ==========================================
    std::cout << "=== Test 1: Vector in Loop ===\n";
    
    {
        Timer timer;
        sink = process_bad_allocation(iterations, elements);
        std::cout << "BAD (new allocation each iter): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = process_reuse_buffer(iterations, elements);
        std::cout << "GOOD (reuse with clear): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = process_resize_pattern(iterations, elements);
        std::cout << "BETTER (resize once): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Test 2: String building
    // ==========================================
    std::cout << "=== Test 2: String Building ===\n";
    
    const int string_parts = 10000;
    
    {
        Timer timer;
        std::string result = build_string_bad(string_parts);
        std::cout << "BAD (+ concatenation): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms";
        std::cout << " (length: " << result.length() << ")\n";
    }
    
    {
        Timer timer;
        std::string result = build_string_good(string_parts);
        std::cout << "GOOD (reserve + append): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms";
        std::cout << " (length: " << result.length() << ")\n";
    }
    
    {
        Timer timer;
        std::string result = build_string_stream(string_parts);
        std::cout << "BEST (ostringstream): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms";
        std::cout << " (length: " << result.length() << ")\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Test 3: Object creation
    // ==========================================
    std::cout << "=== Test 3: Object Creation ===\n";
    
    const size_t object_size = 10000;
    
    {
        Timer timer;
        sink = process_objects_bad(iterations, object_size);
        std::cout << "BAD (create each iter): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        sink = process_objects_good(iterations, object_size);
        std::cout << "GOOD (reuse object): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Test 4: Transform functions
    // ==========================================
    std::cout << "=== Test 4: Transform Functions ===\n";
    
    std::vector<int> input(elements);
    for (int i = 0; i < elements; ++i) input[i] = i;
    
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            auto result = transform_bad(input);
            sink = result.size();
        }
        std::cout << "BAD (return new vector): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    
    {
        std::vector<int> output;
        output.reserve(elements);  // Pre-allocate
        
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            transform_good(input, output);
            sink = output.size();
        }
        std::cout << "GOOD (output parameter): " << std::fixed 
                  << std::setprecision(2) << timer.elapsed_ms() << " ms\n";
    }
    
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            std::vector<int> data = input;  // Copy once
            transform_inplace(data);
            sink = data.size();
        }
        std::cout << "Note: In-place requires initial copy\n";
    }
    std::cout << "\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Hot Loop Allocation Rules:\n\n";
    
    std::cout << "1. MOVE ALLOCATIONS OUTSIDE LOOPS:\n";
    std::cout << "   // Bad\n";
    std::cout << "   for (...) {\n";
    std::cout << "       vector<int> temp(size);  // Alloc every iter!\n";
    std::cout << "   }\n";
    std::cout << "   // Good\n";
    std::cout << "   vector<int> temp(size);  // Alloc once\n";
    std::cout << "   for (...) {\n";
    std::cout << "       temp.clear();  // Just reset size\n";
    std::cout << "   }\n\n";
    
    std::cout << "2. USE reserve() FOR KNOWN SIZES:\n";
    std::cout << "   vector<int> v;\n";
    std::cout << "   v.reserve(expected_size);  // Single allocation\n\n";
    
    std::cout << "3. REUSE OBJECTS (reset instead of recreate):\n";
    std::cout << "   Object obj(...);\n";
    std::cout << "   for (...) {\n";
    std::cout << "       obj.reset();  // Reset state\n";
    std::cout << "       obj.process(...);\n";
    std::cout << "   }\n\n";
    
    std::cout << "4. STRING BUILDING:\n";
    std::cout << "   - Use reserve() + append()\n";
    std::cout << "   - Or std::ostringstream\n";
    std::cout << "   - Avoid + concatenation in loops\n\n";
    
    std::cout << "5. OUTPUT PARAMETERS:\n";
    std::cout << "   - Pass output buffer to functions\n";
    std::cout << "   - Caller controls allocation\n";
    std::cout << "   - Can reuse same buffer\n";
    
    return 0;
}
