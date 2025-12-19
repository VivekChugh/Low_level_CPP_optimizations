/**
 * Branch Prediction Cost on Random Data
 * 
 * This code demonstrates the severe performance penalty of branch mispredictions.
 * Modern CPUs use branch predictors to guess which way a branch will go,
 * allowing them to speculatively execute instructions. When the prediction
 * is wrong, the CPU must discard speculative work (10-25 cycle penalty).
 * 
 * Key concepts:
 * - Branch predictor learns patterns in branch outcomes
 * - Sorted data: Branch becomes predictable (all false, then all true)
 * - Random data: Branch is unpredictable (~50% misprediction rate)
 * - Misprediction penalty: 10-25 CPU cycles per miss
 * 
 * Expected result: Random data is significantly slower than sorted data
 * 
 * Compile: g++ -O3 -std=c++17 -o 01_branch_prediction 01_branch_prediction.cpp
 * Run: ./01_branch_prediction
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <iomanip>

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
 * Count elements greater than threshold using a branch
 * 
 * The 'if' statement creates a branch in the machine code.
 * The branch predictor tries to guess whether we take the branch or not.
 * 
 * For sorted data: The pattern is predictable
 *   [1,2,3,4,5,6,7,8,9,10] with threshold=5
 *   Results: F,F,F,F,F,T,T,T,T,T (all false, then all true)
 *   Branch predictor learns this pattern quickly
 * 
 * For random data: The pattern is unpredictable
 *   [7,2,9,1,5,8,3,6,4,10] with threshold=5
 *   Results: T,F,T,F,F,T,F,T,F,T (random)
 *   Branch predictor guesses wrong ~50% of the time
 */
long long count_greater_than_branched(const std::vector<int>& data, int threshold) {
    long long count = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        // This 'if' creates a branch
        // CPU must predict: will we increment count or not?
        if (data[i] > threshold) {
            count++;
        }
    }
    
    return count;
}

/**
 * Branchless version using arithmetic
 * 
 * Instead of branching, we use the fact that:
 * - (data[i] > threshold) evaluates to 0 or 1
 * - We can add this directly to count
 * 
 * This eliminates the branch entirely - no mispredictions possible!
 */
long long count_greater_than_branchless(const std::vector<int>& data, int threshold) {
    long long count = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        // No branch! The comparison result (0 or 1) is added directly
        count += (data[i] > threshold);
    }
    
    return count;
}

/**
 * Another branchless technique using bit manipulation
 */
long long count_greater_than_bitmask(const std::vector<int>& data, int threshold) {
    long long count = 0;
    
    for (size_t i = 0; i < data.size(); ++i) {
        // (data[i] - threshold - 1) is negative if data[i] <= threshold
        // Right-shifting a negative number fills with 1s (arithmetic shift)
        // So ~(x >> 31) gives us 1 if x >= 0, 0 if x < 0
        int diff = data[i] - threshold - 1;
        count += ~(diff >> 31) & 1;
    }
    
    return count;
}

int main() {
    std::cout << "=== Branch Prediction Cost Demo ===\n\n";
    
    // Configuration
    const size_t N = 100000000;  // 100 million elements
    const int MAX_VALUE = 256;
    const int THRESHOLD = MAX_VALUE / 2;  // 128 - about 50% will be greater
    const int num_runs = 5;
    
    std::cout << "Array size: " << N << " elements\n";
    std::cout << "Value range: 0 to " << MAX_VALUE - 1 << "\n";
    std::cout << "Threshold: " << THRESHOLD << "\n\n";
    
    // Create random data
    std::cout << "Generating random data...\n";
    std::vector<int> random_data(N);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, MAX_VALUE - 1);
    
    for (size_t i = 0; i < N; ++i) {
        random_data[i] = dist(gen);
    }
    
    // Create sorted copy
    std::cout << "Creating sorted copy...\n";
    std::vector<int> sorted_data = random_data;
    std::sort(sorted_data.begin(), sorted_data.end());
    
    std::cout << "Data prepared.\n\n";
    
    // Warm-up
    sink = count_greater_than_branched(random_data, THRESHOLD);
    sink = count_greater_than_branched(sorted_data, THRESHOLD);
    
    // Test 1: Branched version on SORTED data
    std::cout << "=== Test 1: Branched Code on SORTED Data ===\n";
    std::cout << "(Branch predictor can learn the pattern)\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = count_greater_than_branched(sorted_data, THRESHOLD);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Count: " << result << "\n\n";
    }
    
    // Test 2: Branched version on RANDOM data
    std::cout << "=== Test 2: Branched Code on RANDOM Data ===\n";
    std::cout << "(Branch predictor fails ~50% of the time)\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = count_greater_than_branched(random_data, THRESHOLD);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Count: " << result << "\n\n";
    }
    
    // Test 3: Branchless version on RANDOM data
    std::cout << "=== Test 3: Branchless Code on RANDOM Data ===\n";
    std::cout << "(No branches = no mispredictions)\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = count_greater_than_branchless(random_data, THRESHOLD);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Count: " << result << "\n\n";
    }
    
    // Test 4: Branchless on sorted (to show it's consistent)
    std::cout << "=== Test 4: Branchless Code on SORTED Data ===\n";
    std::cout << "(For comparison - branchless is consistent)\n";
    {
        double total_time = 0;
        long long result;
        
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = count_greater_than_branchless(sorted_data, THRESHOLD);
            total_time += timer.elapsed_ms();
            sink = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Count: " << result << "\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Why is random data slower with branches?\n\n";
    
    std::cout << "1. BRANCH PREDICTION:\n";
    std::cout << "   - CPU guesses which way a branch will go\n";
    std::cout << "   - Starts executing instructions speculatively\n";
    std::cout << "   - If wrong, must discard work and restart\n\n";
    
    std::cout << "2. SORTED DATA:\n";
    std::cout << "   - Pattern: FFFFFFFFFTTTTTTTTT\n";
    std::cout << "   - Predictor learns: 'keep predicting same as last'\n";
    std::cout << "   - Only misses at the transition point\n\n";
    
    std::cout << "3. RANDOM DATA:\n";
    std::cout << "   - Pattern: TFFTFTTTFFTFTTFT\n";
    std::cout << "   - No learnable pattern\n";
    std::cout << "   - ~50% misprediction rate\n";
    std::cout << "   - Each miss costs 10-25 cycles\n\n";
    
    std::cout << "4. BRANCHLESS CODE:\n";
    std::cout << "   - Eliminates the branch entirely\n";
    std::cout << "   - Uses arithmetic/bit manipulation instead\n";
    std::cout << "   - Consistent performance regardless of data pattern\n\n";
    
    std::cout << "When to use branchless code:\n";
    std::cout << "- Unpredictable data patterns\n";
    std::cout << "- Hot loops where branches are the bottleneck\n";
    std::cout << "- When profiling shows high branch misprediction rate\n";
    
    return 0;
}
