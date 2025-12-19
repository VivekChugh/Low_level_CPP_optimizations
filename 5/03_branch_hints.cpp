/**
 * Using Branch Hints - [[likely]] and [[unlikely]]
 * 
 * This code demonstrates how to use branch hints to help the compiler
 * optimize code layout for expected execution paths.
 * 
 * Key concepts:
 * - [[likely]] / [[unlikely]] (C++20) hint which branch is more probable
 * - __builtin_expect (GCC/Clang) is the older equivalent
 * - Compiler arranges code so the likely path has fewer jumps
 * - Helps instruction cache locality (likely path is sequential)
 * - Does NOT affect branch prediction at runtime (that's CPU's job)
 * 
 * Use cases:
 * - Error handling paths (unlikely to fail)
 * - Input validation (usually valid)
 * - Hot paths in performance-critical code
 * 
 * Compile: g++ -O3 -std=c++20 -o 03_branch_hints 03_branch_hints.cpp
 * (Use -std=c++17 with __builtin_expect if C++20 not available)
 * 
 * Run: ./03_branch_hints
 * 
 * To see assembly: g++ -S -O3 -std=c++20 03_branch_hints.cpp -o hints.s
 */

#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <cstdint>

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
volatile long long sink;

/**
 * Macros for branch hints (for C++17 and earlier)
 * 
 * __builtin_expect(expr, expected) tells the compiler that
 * 'expr' is expected to equal 'expected' most of the time.
 */
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/**
 * Version 1: No branch hints
 * 
 * Compiler arranges code in source order.
 * May or may not be optimal for the actual execution pattern.
 */
long long process_no_hints(const std::vector<int>& data, int rare_value) {
    long long fast_path_sum = 0;
    long long slow_path_sum = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == rare_value) {
            // Rare case - expensive computation
            slow_path_sum += data[i] * data[i] * data[i];
            slow_path_sum += i * 17;
        } else {
            // Common case - simple computation
            fast_path_sum += data[i];
        }
    }
    
    return fast_path_sum + slow_path_sum;
}

/**
 * Version 2: Using __builtin_expect (C++11 compatible)
 * 
 * We tell the compiler that (data[i] == rare_value) is unlikely.
 * Compiler will:
 * - Put fast path code inline (sequential)
 * - Put slow path code out-of-line or after a forward jump
 */
long long process_builtin_expect(const std::vector<int>& data, int rare_value) {
    long long fast_path_sum = 0;
    long long slow_path_sum = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        // UNLIKELY() tells compiler this condition is rarely true
        if (UNLIKELY(data[i] == rare_value)) {
            // Rare case - compiler may move this code out of the hot path
            slow_path_sum += data[i] * data[i] * data[i];
            slow_path_sum += i * 17;
        } else {
            // Common case - this code stays in the hot path
            fast_path_sum += data[i];
        }
    }
    
    return fast_path_sum + slow_path_sum;
}

/**
 * Version 3: Using [[likely]] and [[unlikely]] (C++20)
 * 
 * This is the modern, standardized way to provide branch hints.
 * More readable and portable than __builtin_expect.
 */
#if __cplusplus >= 202002L
long long process_cpp20_hints(const std::vector<int>& data, int rare_value) {
    long long fast_path_sum = 0;
    long long slow_path_sum = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        if (data[i] == rare_value) [[unlikely]] {
            // Rare case - marked as unlikely
            slow_path_sum += data[i] * data[i] * data[i];
            slow_path_sum += i * 17;
        } else [[likely]] {
            // Common case - marked as likely
            fast_path_sum += data[i];
        }
    }
    
    return fast_path_sum + slow_path_sum;
}
#endif

/**
 * A more realistic example: Error checking with early return
 * 
 * Common pattern: Check for errors first, then do the real work.
 * Errors are rare, so mark them as unlikely.
 */
int process_with_validation_no_hint(int* ptr, int size) {
    // Validation checks
    if (ptr == nullptr) {
        return -1;  // Error: null pointer
    }
    
    if (size <= 0) {
        return -2;  // Error: invalid size
    }
    
    if (size > 1000000) {
        return -3;  // Error: size too large
    }
    
    // Actual work (only reached if all checks pass)
    int sum = 0;
    for (int i = 0; i < size; ++i) {
        sum += ptr[i];
    }
    
    return sum;
}

int process_with_validation_hinted(int* ptr, int size) {
    // Validation checks - all marked as unlikely
    if (UNLIKELY(ptr == nullptr)) {
        return -1;  // Error: null pointer (rare)
    }
    
    if (UNLIKELY(size <= 0)) {
        return -2;  // Error: invalid size (rare)
    }
    
    if (UNLIKELY(size > 1000000)) {
        return -3;  // Error: size too large (rare)
    }
    
    // Actual work - this is the likely path
    int sum = 0;
    for (int i = 0; i < size; ++i) {
        sum += ptr[i];
    }
    
    return sum;
}

/**
 * Example: Switch statement with likely case
 */
enum class MessageType : int {
    HEARTBEAT = 0,      // Very common (99%)
    DATA = 1,           // Occasional
    ERROR = 2,          // Rare
    SHUTDOWN = 3        // Very rare
};

long long process_message_no_hint(MessageType type, long long value) {
    switch (type) {
        case MessageType::HEARTBEAT:
            return value + 1;  // Simple increment
        case MessageType::DATA:
            return value * 2;  // Process data
        case MessageType::ERROR:
            return -value;     // Error handling
        case MessageType::SHUTDOWN:
            return 0;          // Shutdown
        default:
            return -1;
    }
}

long long process_message_hinted(MessageType type, long long value) {
    // Check for the common case first with likely hint
    if (LIKELY(type == MessageType::HEARTBEAT)) {
        return value + 1;
    }
    
    // Less common cases
    switch (type) {
        case MessageType::DATA:
            return value * 2;
        case MessageType::ERROR:
            return -value;
        case MessageType::SHUTDOWN:
            return 0;
        default:
            return -1;
    }
}

int main() {
    std::cout << "=== Branch Hints Demo ===\n\n";
    
    // Configuration
    const size_t N = 100000000;  // 100 million elements
    const int RARE_VALUE = 999;  // Only ~0.01% of values will match
    const int VALUE_RANGE = 1000;
    const int num_runs = 5;
    
    std::cout << "Array size: " << N << " elements\n";
    std::cout << "Rare value: " << RARE_VALUE << "\n";
    std::cout << "Expected rare occurrence: ~" << (100.0 / VALUE_RANGE) << "%\n\n";
    
    // Create data with rare values
    std::cout << "Generating data...\n";
    std::vector<int> data(N);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, VALUE_RANGE - 1);
    
    size_t rare_count = 0;
    for (size_t i = 0; i < N; ++i) {
        data[i] = dist(gen);
        if (data[i] == RARE_VALUE) rare_count++;
    }
    
    std::cout << "Actual rare occurrences: " << rare_count 
              << " (" << (100.0 * rare_count / N) << "%)\n\n";
    
    // Warm-up
    sink = process_no_hints(data, RARE_VALUE);
    sink = process_builtin_expect(data, RARE_VALUE);
    
    // Test 1: No hints
    std::cout << "=== Test 1: No Branch Hints ===\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = process_no_hints(data, RARE_VALUE);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 2: __builtin_expect hints
    std::cout << "=== Test 2: __builtin_expect Hints ===\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = process_builtin_expect(data, RARE_VALUE);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 3: C++20 [[likely]]/[[unlikely]] hints
    #if __cplusplus >= 202002L
    std::cout << "=== Test 3: C++20 [[likely]]/[[unlikely]] Hints ===\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = process_cpp20_hints(data, RARE_VALUE);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    #else
    std::cout << "=== Test 3: Skipped (requires C++20) ===\n\n";
    #endif
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "What branch hints do:\n\n";
    
    std::cout << "1. CODE LAYOUT:\n";
    std::cout << "   - Likely path: Kept inline, sequential execution\n";
    std::cout << "   - Unlikely path: Moved to end, requires jump\n";
    std::cout << "   - Improves instruction cache locality\n\n";
    
    std::cout << "2. WHAT THEY DON'T DO:\n";
    std::cout << "   - Don't affect CPU branch prediction\n";
    std::cout << "   - Don't guarantee performance improvement\n";
    std::cout << "   - Don't change program semantics\n\n";
    
    std::cout << "3. WHEN TO USE:\n";
    std::cout << "   - Error checking (errors are rare)\n";
    std::cout << "   - Input validation (valid input is common)\n";
    std::cout << "   - Known-skewed conditionals (99%/1% splits)\n";
    std::cout << "   - Hot loops where code layout matters\n\n";
    
    std::cout << "4. SYNTAX:\n";
    std::cout << "   C++20:  if (condition) [[unlikely]] { ... }\n";
    std::cout << "   GCC:    if (__builtin_expect(condition, 0)) { ... }\n";
    std::cout << "   Macro:  if (UNLIKELY(condition)) { ... }\n\n";
    
    std::cout << "Note: Modern compilers are smart!\n";
    std::cout << "They often figure out branch probabilities from:\n";
    std::cout << "- Profile-guided optimization (PGO)\n";
    std::cout << "- Heuristics (comparisons with 0, null checks, etc.)\n";
    std::cout << "- Code patterns (error returns, assertions)\n";
    
    return 0;
}
