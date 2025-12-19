/**
 * Optimal Struct Padding - Field Ordering and Alignment
 * 
 * This code demonstrates how the order of fields in a struct affects
 * its memory layout, size, and cache efficiency due to alignment padding.
 * 
 * Key concepts:
 * - Types have alignment requirements (char=1, int=4, double=8, etc.)
 * - Compiler adds padding to satisfy alignment
 * - Poor field ordering wastes memory with internal padding
 * - Good field ordering minimizes padding and struct size
 * - Smaller structs = more objects per cache line = better performance
 * 
 * Rule of thumb: Order fields from largest to smallest alignment
 * 
 * Compile: g++ -O3 -std=c++17 -o 02_struct_padding 02_struct_padding.cpp
 * Run: ./02_struct_padding
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstddef>  // for offsetof

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
 * BAD: Poorly ordered struct
 * 
 * Memory layout (assuming 64-bit system):
 * Offset  Field     Size   Notes
 * ------  -----     ----   -----
 * 0       c1        1      char
 * 1-7     [padding] 7      Pad to align double (8-byte boundary)
 * 8       d         8      double (requires 8-byte alignment)
 * 16      i         4      int
 * 20-23   [padding] 4      Pad to align next char? No, but struct alignment
 * 24      c2        1      char
 * 25-31   [padding] 7      Pad to make struct size multiple of max alignment (8)
 * 
 * Total: 32 bytes (but only 14 bytes of actual data!)
 * Waste: 18 bytes of padding (56% waste!)
 */
struct BadStruct {
    char c1;      // 1 byte
    double d;     // 8 bytes (needs 8-byte alignment)
    int i;        // 4 bytes
    char c2;      // 1 byte
};

/**
 * GOOD: Well-ordered struct
 * 
 * Memory layout:
 * Offset  Field     Size   Notes
 * ------  -----     ----   -----
 * 0       d         8      double (8-byte aligned at start)
 * 8       i         4      int (4-byte aligned, no padding needed)
 * 12      c1        1      char (1-byte aligned, no padding)
 * 13      c2        1      char (1-byte aligned, no padding)
 * 14-15   [padding] 2      Pad to make struct size multiple of 8
 * 
 * Total: 16 bytes (14 bytes data + 2 bytes padding)
 * Waste: 2 bytes of padding (12.5% waste)
 */
struct GoodStruct {
    double d;     // 8 bytes (largest alignment first)
    int i;        // 4 bytes
    char c1;      // 1 byte
    char c2;      // 1 byte
};

/**
 * Another example with more types
 */
struct BadStruct2 {
    char a;       // 1 byte
    int b;        // 4 bytes (needs 4-byte alignment)
    char c;       // 1 byte
    long long d;  // 8 bytes (needs 8-byte alignment)
    char e;       // 1 byte
    short f;      // 2 bytes (needs 2-byte alignment)
};

struct GoodStruct2 {
    long long d;  // 8 bytes (largest first)
    int b;        // 4 bytes
    short f;      // 2 bytes
    char a;       // 1 byte
    char c;       // 1 byte
    char e;       // 1 byte
    // 3 bytes padding to align to 8
};

/**
 * Using packed attribute (USE WITH CAUTION!)
 * 
 * This removes padding but can cause:
 * - Misaligned access (slow or crash on some architectures)
 * - Breaks strict aliasing rules
 * - Not portable
 */
#pragma pack(push, 1)
struct PackedStruct {
    char c1;
    double d;
    int i;
    char c2;
};
#pragma pack(pop)

/**
 * Cache-line aligned struct for avoiding false sharing
 */
struct alignas(64) CacheLineAligned {
    double d;
    int i;
    char c1;
    char c2;
    // Padding to 64 bytes
};

/**
 * Print struct layout details
 */
template<typename T>
void print_struct_info(const char* name) {
    std::cout << name << ":\n";
    std::cout << "  sizeof: " << sizeof(T) << " bytes\n";
    std::cout << "  alignof: " << alignof(T) << " bytes\n";
}

/**
 * Benchmark array traversal - smaller structs = more per cache line = faster
 */
template<typename T>
double benchmark_traversal(std::vector<T>& data, int num_runs) {
    double total_time = 0;
    
    for (int run = 0; run < num_runs; ++run) {
        Timer timer;
        
        long long sum = 0;
        for (size_t i = 0; i < data.size(); ++i) {
            // Access the double field (named 'd' in all structs)
            sum += static_cast<long long>(data[i].d);
        }
        
        total_time += timer.elapsed_ms();
        sink = sum;
    }
    
    return total_time / num_runs;
}

int main() {
    std::cout << "=== Struct Padding and Field Ordering Demo ===\n\n";
    
    // Show basic type alignments
    std::cout << "=== Basic Type Alignments ===\n";
    std::cout << "Type        Size    Alignment\n";
    std::cout << "----        ----    ---------\n";
    std::cout << "char        " << sizeof(char) << "       " << alignof(char) << "\n";
    std::cout << "short       " << sizeof(short) << "       " << alignof(short) << "\n";
    std::cout << "int         " << sizeof(int) << "       " << alignof(int) << "\n";
    std::cout << "long        " << sizeof(long) << "       " << alignof(long) << "\n";
    std::cout << "long long   " << sizeof(long long) << "       " << alignof(long long) << "\n";
    std::cout << "float       " << sizeof(float) << "       " << alignof(float) << "\n";
    std::cout << "double      " << sizeof(double) << "       " << alignof(double) << "\n";
    std::cout << "void*       " << sizeof(void*) << "       " << alignof(void*) << "\n\n";
    
    // Show struct sizes
    std::cout << "=== Struct Size Comparison ===\n\n";
    
    std::cout << "--- Example 1: char, double, int, char ---\n";
    print_struct_info<BadStruct>("BadStruct (char, double, int, char)");
    print_struct_info<GoodStruct>("GoodStruct (double, int, char, char)");
    std::cout << "Savings: " << sizeof(BadStruct) - sizeof(GoodStruct) << " bytes per object\n";
    std::cout << "Percentage: " << std::fixed << std::setprecision(1) 
              << (1.0 - (double)sizeof(GoodStruct) / sizeof(BadStruct)) * 100 << "% smaller\n\n";
    
    std::cout << "--- Example 2: Mixed types ---\n";
    print_struct_info<BadStruct2>("BadStruct2");
    print_struct_info<GoodStruct2>("GoodStruct2");
    std::cout << "Savings: " << sizeof(BadStruct2) - sizeof(GoodStruct2) << " bytes per object\n\n";
    
    std::cout << "--- Packed struct (use with caution!) ---\n";
    print_struct_info<PackedStruct>("PackedStruct");
    std::cout << "Warning: Misaligned access may be slow or crash!\n\n";
    
    std::cout << "--- Cache-line aligned struct ---\n";
    print_struct_info<CacheLineAligned>("CacheLineAligned");
    std::cout << "Use for thread-local data to avoid false sharing\n\n";
    
    // Show detailed memory layout
    std::cout << "=== Detailed Memory Layout ===\n\n";
    
    std::cout << "BadStruct field offsets:\n";
    std::cout << "  c1: offset " << offsetof(BadStruct, c1) << ", size 1\n";
    std::cout << "  d:  offset " << offsetof(BadStruct, d) << ", size 8\n";
    std::cout << "  i:  offset " << offsetof(BadStruct, i) << ", size 4\n";
    std::cout << "  c2: offset " << offsetof(BadStruct, c2) << ", size 1\n";
    std::cout << "  Total: " << sizeof(BadStruct) << " bytes\n\n";
    
    std::cout << "GoodStruct field offsets:\n";
    std::cout << "  d:  offset " << offsetof(GoodStruct, d) << ", size 8\n";
    std::cout << "  i:  offset " << offsetof(GoodStruct, i) << ", size 4\n";
    std::cout << "  c1: offset " << offsetof(GoodStruct, c1) << ", size 1\n";
    std::cout << "  c2: offset " << offsetof(GoodStruct, c2) << ", size 1\n";
    std::cout << "  Total: " << sizeof(GoodStruct) << " bytes\n\n";
    
    // Visual representation
    std::cout << "=== Visual Memory Layout ===\n\n";
    
    std::cout << "BadStruct (32 bytes):\n";
    std::cout << "|c1|----padding----|  d (8 bytes)  |  i  |--|-c2|---padding---|\n";
    std::cout << " 0  1             7 8            15 16  19 20 21 24          31\n\n";
    
    std::cout << "GoodStruct (16 bytes):\n";
    std::cout << "|  d (8 bytes)  |  i  |c1|c2|--|\n";
    std::cout << " 0             7 8   11 12 13 14 15\n\n";
    
    // Benchmark impact
    std::cout << "=== Performance Impact ===\n\n";
    
    const size_t N = 10000000;  // 10 million elements
    const int num_runs = 10;
    
    std::cout << "Array of " << N << " elements:\n";
    std::cout << "  BadStruct array:  " << (N * sizeof(BadStruct)) / (1024.0 * 1024.0) << " MB\n";
    std::cout << "  GoodStruct array: " << (N * sizeof(GoodStruct)) / (1024.0 * 1024.0) << " MB\n\n";
    
    // Calculate cache line efficiency
    const size_t CACHE_LINE = 64;
    std::cout << "Objects per cache line (64 bytes):\n";
    std::cout << "  BadStruct:  " << CACHE_LINE / sizeof(BadStruct) << " objects\n";
    std::cout << "  GoodStruct: " << CACHE_LINE / sizeof(GoodStruct) << " objects\n\n";
    
    // Create test data
    std::vector<BadStruct> bad_data(N);
    std::vector<GoodStruct> good_data(N);
    
    // Initialize
    for (size_t i = 0; i < N; ++i) {
        bad_data[i].d = static_cast<double>(i);
        bad_data[i].i = static_cast<int>(i);
        bad_data[i].c1 = 'a';
        bad_data[i].c2 = 'b';
        
        good_data[i].d = static_cast<double>(i);
        good_data[i].i = static_cast<int>(i);
        good_data[i].c1 = 'a';
        good_data[i].c2 = 'b';
    }
    
    // Warm-up
    benchmark_traversal(bad_data, 2);
    benchmark_traversal(good_data, 2);
    
    // Benchmark
    std::cout << "Traversal benchmark:\n";
    
    double bad_time = benchmark_traversal(bad_data, num_runs);
    std::cout << "  BadStruct:  " << std::fixed << std::setprecision(2) << bad_time << " ms\n";
    
    double good_time = benchmark_traversal(good_data, num_runs);
    std::cout << "  GoodStruct: " << std::fixed << std::setprecision(2) << good_time << " ms\n";
    
    std::cout << "\nSpeedup: " << std::setprecision(2) << bad_time / good_time << "x\n\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Rules for optimal struct layout:\n";
    std::cout << "1. Order fields from largest alignment to smallest\n";
    std::cout << "2. Group same-sized fields together\n";
    std::cout << "3. Use 'alignas' for specific alignment needs\n";
    std::cout << "4. Avoid '#pragma pack' unless absolutely necessary\n";
    std::cout << "5. Use 'sizeof' and 'offsetof' to verify layout\n\n";
    
    std::cout << "Typical alignment order:\n";
    std::cout << "  1. long long, double, pointers (8 bytes)\n";
    std::cout << "  2. int, float (4 bytes)\n";
    std::cout << "  3. short (2 bytes)\n";
    std::cout << "  4. char, bool (1 byte)\n\n";
    
    std::cout << "Tools for analysis:\n";
    std::cout << "  - pahole (Linux): Shows struct holes\n";
    std::cout << "  - Compiler warnings: -Wpadded (GCC/Clang)\n";
    std::cout << "  - static_assert for compile-time size checks\n";
    
    return 0;
}
