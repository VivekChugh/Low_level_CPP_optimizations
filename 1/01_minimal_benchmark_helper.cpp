/**
 * Minimal Benchmark Helper
 * 
 * This code demonstrates the fundamental principle of "Measurement First" in optimization.
 * It implements a minimal timing function using std::chrono::high_resolution_clock.
 * 
 * Key concepts:
 * - Using high_resolution_clock for precise timing
 * - Warm-up phase to stabilize CPU frequency and prime caches
 * - Running multiple iterations for statistical significance
 * 
 * Compile: g++ -O3 -std=c++17 -o 01_minimal_benchmark_helper 01_minimal_benchmark_helper.cpp
 * Run: ./01_minimal_benchmark_helper
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>

// A helper class to measure execution time of code blocks
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double, std::milli>;  // milliseconds
    
private:
    TimePoint start_time;
    
public:
    // Constructor starts the timer automatically
    Timer() : start_time(Clock::now()) {}
    
    // Reset the timer to current time
    void reset() {
        start_time = Clock::now();
    }
    
    // Get elapsed time in milliseconds
    double elapsed_ms() const {
        auto end_time = Clock::now();
        Duration elapsed = end_time - start_time;
        return elapsed.count();
    }
    
    // Get elapsed time in microseconds
    double elapsed_us() const {
        return elapsed_ms() * 1000.0;
    }
};

// Template function to benchmark any callable
// Includes warm-up phase and multiple iterations
template<typename Func>
double benchmark(Func&& func, int warmup_iterations = 3, int measure_iterations = 10) {
    // Warm-up phase: Run the function a few times to:
    // 1. Stabilize CPU frequency (modern CPUs scale frequency dynamically)
    // 2. Prime instruction and data caches
    // 3. Trigger any lazy initializations
    for (int i = 0; i < warmup_iterations; ++i) {
        func();
    }
    
    // Measurement phase: Collect timing data
    std::vector<double> timings;
    timings.reserve(measure_iterations);
    
    for (int i = 0; i < measure_iterations; ++i) {
        Timer timer;
        func();
        timings.push_back(timer.elapsed_us());
    }
    
    // Calculate and return average time
    double total = std::accumulate(timings.begin(), timings.end(), 0.0);
    return total / timings.size();
}

// Example function to benchmark: compute sum of squares
double compute_sum_of_squares(const std::vector<double>& data) {
    double sum = 0.0;
    for (const auto& val : data) {
        sum += val * val;  // Square each element and accumulate
    }
    return sum;
}

// Prevent compiler from optimizing away the result
// This is a simple technique using volatile
volatile double sink;

int main() {
    std::cout << "=== Minimal Benchmark Helper Demo ===\n\n";
    
    // Create test data
    const size_t N = 1000000;  // 1 million elements
    std::vector<double> data(N);
    
    // Initialize with some values
    for (size_t i = 0; i < N; ++i) {
        data[i] = static_cast<double>(i) * 0.001;
    }
    
    // Method 1: Using the Timer class directly
    std::cout << "Method 1: Direct Timer usage\n";
    {
        Timer timer;
        double result = compute_sum_of_squares(data);
        double elapsed = timer.elapsed_us();
        sink = result;  // Prevent dead-code elimination
        std::cout << "  Single run: " << elapsed << " microseconds\n";
        std::cout << "  Result: " << result << "\n\n";
    }
    
    // Method 2: Using the benchmark helper function
    std::cout << "Method 2: Benchmark helper with warm-up\n";
    {
        double result;
        // Lambda captures 'data' by reference and 'result' to store output
        double avg_time = benchmark([&]() {
            result = compute_sum_of_squares(data);
            sink = result;  // Prevent optimization
        }, 5, 20);  // 5 warm-up iterations, 20 measurement iterations
        
        std::cout << "  Average over 20 runs: " << avg_time << " microseconds\n";
        std::cout << "  Result: " << result << "\n\n";
    }
    
    // Demonstrate the importance of warm-up
    std::cout << "Method 3: Showing warm-up effect\n";
    {
        std::vector<double> cold_timings;
        std::vector<double> warm_timings;
        
        // Cold runs (first few iterations)
        for (int i = 0; i < 5; ++i) {
            Timer timer;
            sink = compute_sum_of_squares(data);
            cold_timings.push_back(timer.elapsed_us());
        }
        
        // Warm runs (after stabilization)
        for (int i = 0; i < 5; ++i) {
            Timer timer;
            sink = compute_sum_of_squares(data);
            warm_timings.push_back(timer.elapsed_us());
        }
        
        std::cout << "  Cold run times (us): ";
        for (auto t : cold_timings) std::cout << t << " ";
        std::cout << "\n";
        
        std::cout << "  Warm run times (us): ";
        for (auto t : warm_timings) std::cout << t << " ";
        std::cout << "\n";
    }
    
    return 0;
}
