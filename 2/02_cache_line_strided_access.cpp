/**
 * Cache Line Size and Strided Access
 * 
 * This code demonstrates how access patterns affect cache efficiency.
 * It compares sequential access vs strided (jumping) access patterns.
 * 
 * Key concepts:
 * - Cache lines are typically 64 bytes on modern CPUs
 * - Sequential access loads data that will be used soon
 * - Strided access wastes cache lines by loading unused data
 * - Large strides defeat the hardware prefetcher
 * - Performance degrades dramatically with larger strides
 * 
 * Expected result: Performance collapses as stride increases
 * 
 * Compile: g++ -O3 -std=c++17 -o 02_strided_access 02_cache_line_strided_access.cpp
 * Run: ./02_strided_access
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cmath>

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
 * Sequential access pattern
 * 
 * Access pattern: [0][1][2][3][4][5][6][7]...
 * 
 * This is the ideal access pattern:
 * - Perfect spatial locality
 * - Prefetcher can predict and pre-load upcoming data
 * - Each cache line is fully utilized
 */
long long sequential_sum(const std::vector<long long>& data) {
    long long sum = 0;
    
    // Access every element in order
    // This is cache-friendly: data[i+1] is adjacent to data[i]
    for (size_t i = 0; i < data.size(); ++i) {
        sum += data[i];
    }
    
    return sum;
}

/**
 * Strided access pattern with configurable stride
 * 
 * Access pattern with stride=8: [0][8][16][24][32]...
 * 
 * Problems with strided access:
 * - Loads a cache line, but only uses one element from it
 * - Rest of the cache line is wasted
 * - Prefetcher may not detect the pattern (especially large strides)
 * - Causes cache pollution - useful data gets evicted
 */
template<size_t STRIDE>
long long strided_sum(const std::vector<long long>& data) {
    long long sum = 0;
    
    // Access every STRIDE-th element
    // Most of each loaded cache line goes unused
    for (size_t i = 0; i < data.size(); i += STRIDE) {
        sum += data[i];
    }
    
    return sum;
}

/**
 * Dynamic stride version for flexible testing
 */
long long strided_sum_dynamic(const std::vector<long long>& data, size_t stride) {
    long long sum = 0;
    
    for (size_t i = 0; i < data.size(); i += stride) {
        sum += data[i];
    }
    
    return sum;
}

/**
 * Benchmark a specific stride and return average time
 */
double benchmark_stride(const std::vector<long long>& data, size_t stride, int num_runs) {
    double total_time = 0;
    
    for (int run = 0; run < num_runs; ++run) {
        Timer timer;
        sink = strided_sum_dynamic(data, stride);
        total_time += timer.elapsed_ms();
    }
    
    return total_time / num_runs;
}

int main() {
    std::cout << "=== Cache Line and Strided Access Demo ===\n\n";
    
    // Configuration
    // Array size chosen to exceed L3 cache (typically 8-32 MB)
    const size_t N = 64 * 1024 * 1024;  // 64 million elements
    const int num_runs = 5;
    
    // Calculate sizes
    const size_t element_size = sizeof(long long);
    const size_t total_bytes = N * element_size;
    const size_t cache_line_size = 64;  // bytes
    const size_t elements_per_cache_line = cache_line_size / element_size;
    
    std::cout << "Array configuration:\n";
    std::cout << "  Elements: " << N << "\n";
    std::cout << "  Element size: " << element_size << " bytes\n";
    std::cout << "  Total size: " << total_bytes / (1024.0 * 1024.0) << " MB\n";
    std::cout << "  Cache line size: " << cache_line_size << " bytes\n";
    std::cout << "  Elements per cache line: " << elements_per_cache_line << "\n\n";
    
    // Create and initialize data
    std::cout << "Allocating and initializing array...\n";
    std::vector<long long> data(N);
    for (size_t i = 0; i < N; ++i) {
        data[i] = static_cast<long long>(i % 1000);
    }
    
    // Warm-up
    std::cout << "Warming up...\n\n";
    sink = sequential_sum(data);
    sink = strided_sum_dynamic(data, 16);
    
    // Test different strides
    std::vector<size_t> strides = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
    
    std::cout << "=== Benchmark Results ===\n";
    std::cout << std::setw(10) << "Stride" 
              << std::setw(15) << "Time (ms)"
              << std::setw(15) << "Elements"
              << std::setw(20) << "Throughput (M/s)"
              << std::setw(15) << "Relative"
              << "\n";
    std::cout << std::string(75, '-') << "\n";
    
    double baseline_time = 0;
    
    for (size_t stride : strides) {
        // Calculate how many elements we actually touch
        size_t elements_touched = (N + stride - 1) / stride;
        
        // Benchmark this stride
        double avg_time = benchmark_stride(data, stride, num_runs);
        
        // Store baseline (stride=1)
        if (stride == 1) {
            baseline_time = avg_time;
        }
        
        // Calculate throughput (millions of elements per second)
        double throughput = (elements_touched / 1000000.0) / (avg_time / 1000.0);
        
        // Calculate relative slowdown
        // Normalize by elements touched - compare "work per element"
        double normalized_time = avg_time / elements_touched;
        double baseline_normalized = baseline_time / N;
        double relative = normalized_time / baseline_normalized;
        
        std::cout << std::setw(10) << stride
                  << std::setw(15) << std::fixed << std::setprecision(2) << avg_time
                  << std::setw(15) << elements_touched
                  << std::setw(20) << std::setprecision(1) << throughput
                  << std::setw(14) << std::setprecision(1) << relative << "x"
                  << "\n";
    }
    
    // Detailed explanation
    std::cout << "\n=== Analysis ===\n\n";
    
    std::cout << "Why does performance degrade with larger strides?\n\n";
    
    std::cout << "1. CACHE LINE WASTE:\n";
    std::cout << "   - Each cache line holds " << elements_per_cache_line << " elements\n";
    std::cout << "   - Stride 1: Uses all " << elements_per_cache_line << " elements per line (100%)\n";
    std::cout << "   - Stride 8: Uses 1 of " << elements_per_cache_line << " elements per line (12.5%)\n";
    std::cout << "   - Stride 64: Uses 1 element, skips to next cache line entirely\n\n";
    
    std::cout << "2. PREFETCHER CONFUSION:\n";
    std::cout << "   - Sequential access: Prefetcher loads next cache lines automatically\n";
    std::cout << "   - Small strides: Prefetcher may still detect pattern\n";
    std::cout << "   - Large strides: Pattern becomes unpredictable, prefetcher gives up\n\n";
    
    std::cout << "3. TLB (Translation Lookaside Buffer) PRESSURE:\n";
    std::cout << "   - Large strides touch more memory pages\n";
    std::cout << "   - TLB maps virtual to physical addresses\n";
    std::cout << "   - More pages = more TLB misses = slower address translation\n\n";
    
    std::cout << "4. MEMORY BANDWIDTH WASTE:\n";
    std::cout << "   - Memory bus transfers entire cache lines\n";
    std::cout << "   - Stride 64: Load 64 bytes, use only 8 bytes = 87.5% waste\n";
    std::cout << "   - This effectively reduces available memory bandwidth\n\n";
    
    // Visual representation
    std::cout << "=== Visual: Cache Line Usage ===\n\n";
    std::cout << "Cache line (64 bytes, 8 elements):\n";
    std::cout << "[0][1][2][3][4][5][6][7]\n\n";
    
    std::cout << "Stride 1 (sequential): Uses all elements\n";
    std::cout << "[X][X][X][X][X][X][X][X] -> 100% utilization\n\n";
    
    std::cout << "Stride 2: Uses every other element\n";
    std::cout << "[X][ ][X][ ][X][ ][X][ ] -> 50% utilization\n\n";
    
    std::cout << "Stride 4: Uses every 4th element\n";
    std::cout << "[X][ ][ ][ ][X][ ][ ][ ] -> 25% utilization\n\n";
    
    std::cout << "Stride 8: Uses 1 element per cache line\n";
    std::cout << "[X][ ][ ][ ][ ][ ][ ][ ] -> 12.5% utilization\n\n";
    
    std::cout << "Stride 16+: Skips entire cache lines\n";
    std::cout << "[X][ ][ ][ ][ ][ ][ ][ ] [skip] [X][ ][ ][ ][ ][ ][ ][ ]\n";
    
    return 0;
}
