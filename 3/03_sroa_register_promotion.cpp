/**
 * Scalar Replacement of Aggregates (SROA) - Register Promotion
 * 
 * This code demonstrates SROA - an important compiler optimization that
 * eliminates struct/aggregate abstractions by promoting fields to registers.
 * 
 * Key concepts:
 * - Structs are abstractions that may seem to require memory
 * - SROA breaks structs into individual scalar values
 * - These scalars can live in CPU registers (no memory access)
 * - At -O3, small structs often have ZERO runtime overhead
 * 
 * The compiler transformation:
 *   struct P { int x, y; };
 *   P foo() { P p{1, 2}; return p; }
 *   
 * Becomes (conceptually):
 *   pair<int,int> foo() { return {1, 2}; }  // Values in registers
 * 
 * Compile Debug:   g++ -O0 -std=c++17 -o 03_sroa_debug 03_sroa_register_promotion.cpp
 * Compile Release: g++ -O3 -std=c++17 -o 03_sroa_release 03_sroa_register_promotion.cpp
 * 
 * To see assembly (verify SROA):
 *   g++ -S -O3 -std=c++17 03_sroa_register_promotion.cpp -o sroa.s
 *   Or use https://godbolt.org (Compiler Explorer)
 * 
 * Run: ./03_sroa_release
 */

#include <iostream>
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
volatile int sink_int;
volatile float sink_float;

/**
 * Simple 2D Point structure
 * 
 * This looks like it needs memory to store x and y together.
 * But at -O3, the compiler can:
 * 1. Keep x in one register (e.g., eax)
 * 2. Keep y in another register (e.g., edx)
 * 3. Never actually allocate memory for the struct
 */
struct Point2D {
    int x;
    int y;
};

/**
 * A more complex structure for 3D vectors
 */
struct Vector3D {
    float x, y, z;
};

/**
 * Function that creates and returns a Point2D
 * 
 * At -O0:
 * - Allocates stack space for Point2D
 * - Stores x and y to memory
 * - Returns by copying from memory
 * 
 * At -O3 with SROA:
 * - x and y are in registers
 * - No memory allocation for the struct
 * - Return values passed in registers (e.g., rax for x, rdx for y)
 */
Point2D create_point(int a, int b) {
    Point2D p;
    p.x = a + 1;  // Simple computation
    p.y = b + 2;
    return p;
    // At -O3: Effectively returns (a+1, b+2) in registers
}

/**
 * Function that operates on Point2D
 * 
 * At -O3, this entire chain of operations may stay in registers.
 */
inline Point2D add_points(Point2D a, Point2D b) {
    return Point2D{a.x + b.x, a.y + b.y};
}

inline Point2D scale_point(Point2D p, int factor) {
    return Point2D{p.x * factor, p.y * factor};
}

inline int dot_product_2d(Point2D a, Point2D b) {
    return a.x * b.x + a.y * b.y;
}

/**
 * Similar functions for Vector3D
 */
Vector3D create_vector(float x, float y, float z) {
    return Vector3D{x, y, z};
}

inline Vector3D add_vectors(Vector3D a, Vector3D b) {
    return Vector3D{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline float dot_product_3d(Vector3D a, Vector3D b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float magnitude(Vector3D v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

/**
 * Test 1: Using Point2D abstraction
 * 
 * At -O3, this should be as fast as raw scalar operations
 * because SROA eliminates the struct abstraction.
 */
long long test_point_abstraction(long long iterations) {
    long long result = 0;
    
    for (long long i = 0; i < iterations; ++i) {
        // Create points
        Point2D p1 = create_point(static_cast<int>(i % 100), static_cast<int>(i % 50));
        Point2D p2 = create_point(static_cast<int>((i + 1) % 100), static_cast<int>((i + 1) % 50));
        
        // Operations on points
        Point2D sum = add_points(p1, p2);
        Point2D scaled = scale_point(sum, 2);
        
        // Final computation
        result += dot_product_2d(scaled, p1);
    }
    
    return result;
}

/**
 * Test 2: Equivalent operations using raw scalars
 * 
 * This represents what SROA transforms the struct code into.
 * At -O3, test_point_abstraction should match this performance.
 */
long long test_raw_scalars(long long iterations) {
    long long result = 0;
    
    for (long long i = 0; i < iterations; ++i) {
        // Create "points" as separate scalars
        int p1_x = static_cast<int>(i % 100) + 1;
        int p1_y = static_cast<int>(i % 50) + 2;
        int p2_x = static_cast<int>((i + 1) % 100) + 1;
        int p2_y = static_cast<int>((i + 1) % 50) + 2;
        
        // Add
        int sum_x = p1_x + p2_x;
        int sum_y = p1_y + p2_y;
        
        // Scale
        int scaled_x = sum_x * 2;
        int scaled_y = sum_y * 2;
        
        // Dot product
        result += scaled_x * p1_x + scaled_y * p1_y;
    }
    
    return result;
}

/**
 * Test 3: Vector3D operations
 * 
 * 3D vectors are also subject to SROA.
 * The compiler may keep x, y, z in separate registers or use SIMD.
 */
float test_vector3d_abstraction(long long iterations) {
    float result = 0.0f;
    
    for (long long i = 0; i < iterations; ++i) {
        float fi = static_cast<float>(i % 1000) * 0.001f;
        
        Vector3D v1 = create_vector(fi, fi + 1.0f, fi + 2.0f);
        Vector3D v2 = create_vector(fi * 2.0f, fi * 2.0f + 1.0f, fi * 2.0f + 2.0f);
        
        Vector3D sum = add_vectors(v1, v2);
        result += dot_product_3d(sum, v1);
    }
    
    return result;
}

/**
 * Test 4: Raw float scalars (equivalent to Vector3D)
 */
float test_raw_floats(long long iterations) {
    float result = 0.0f;
    
    for (long long i = 0; i < iterations; ++i) {
        float fi = static_cast<float>(i % 1000) * 0.001f;
        
        // Create vectors as scalars
        float v1_x = fi, v1_y = fi + 1.0f, v1_z = fi + 2.0f;
        float v2_x = fi * 2.0f, v2_y = fi * 2.0f + 1.0f, v2_z = fi * 2.0f + 2.0f;
        
        // Add
        float sum_x = v1_x + v2_x;
        float sum_y = v1_y + v2_y;
        float sum_z = v1_z + v2_z;
        
        // Dot product
        result += sum_x * v1_x + sum_y * v1_y + sum_z * v1_z;
    }
    
    return result;
}

/**
 * A case where SROA might NOT apply: pointers to structs
 * 
 * When you take the address of a struct or pass it by pointer,
 * the compiler may need to actually allocate memory.
 */
void modify_point_via_pointer(Point2D* p) {
    // Taking address forces the struct to exist in memory
    p->x += 1;
    p->y += 1;
}

long long test_pointer_access(long long iterations) {
    long long result = 0;
    Point2D p{0, 0};
    
    for (long long i = 0; i < iterations; ++i) {
        // Passing address may prevent SROA
        modify_point_via_pointer(&p);
        result += p.x + p.y;
    }
    
    return result;
}

int main() {
    std::cout << "=== Scalar Replacement of Aggregates (SROA) Demo ===\n\n";
    
    // Detect optimization level
    #ifdef __OPTIMIZE__
        std::cout << "Compiled with OPTIMIZATIONS ENABLED\n";
        std::cout << "SROA should eliminate struct overhead\n\n";
    #else
        std::cout << "Compiled with OPTIMIZATIONS DISABLED\n";
        std::cout << "Structs will have full overhead\n\n";
    #endif
    
    const long long ITERATIONS = 100000000;  // 100 million
    const int num_runs = 5;
    
    std::cout << "Iterations: " << ITERATIONS << "\n";
    std::cout << "Runs per test: " << num_runs << "\n\n";
    
    // Show struct sizes
    std::cout << "Structure sizes:\n";
    std::cout << "  sizeof(Point2D):  " << sizeof(Point2D) << " bytes\n";
    std::cout << "  sizeof(Vector3D): " << sizeof(Vector3D) << " bytes\n\n";
    
    // Warm-up
    sink_int = test_point_abstraction(ITERATIONS / 100);
    sink_int = test_raw_scalars(ITERATIONS / 100);
    
    // Test 1: Point2D abstraction
    std::cout << "--- Test 1: Point2D Struct Abstraction ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_point_abstraction(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink_int = static_cast<int>(result);
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 2: Raw scalars
    std::cout << "--- Test 2: Raw Scalar Operations (baseline) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_raw_scalars(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink_int = static_cast<int>(result);
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "(With SROA, Test 1 should be ~same speed as this)\n\n";
    }
    
    // Test 3: Vector3D abstraction
    std::cout << "--- Test 3: Vector3D Struct Abstraction ---\n";
    {
        double total_time = 0;
        float result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_vector3d_abstraction(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink_float = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 4: Raw floats
    std::cout << "--- Test 4: Raw Float Operations (baseline) ---\n";
    {
        double total_time = 0;
        float result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_raw_floats(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink_float = result;
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "(With SROA, Test 3 should be ~same speed as this)\n\n";
    }
    
    // Test 5: Pointer access (may prevent SROA)
    std::cout << "--- Test 5: Pointer Access (may prevent SROA) ---\n";
    {
        double total_time = 0;
        long long result;
        
        for (int i = 0; i < num_runs; ++i) {
            Timer timer;
            result = test_pointer_access(ITERATIONS);
            total_time += timer.elapsed_ms();
            sink_int = static_cast<int>(result);
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "(Taking address may force memory allocation)\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "What is SROA (Scalar Replacement of Aggregates)?\n";
    std::cout << "- Compiler breaks structs into individual scalar values\n";
    std::cout << "- Scalars can live in CPU registers (fast!)\n";
    std::cout << "- Eliminates memory access for small structs\n";
    std::cout << "- Makes C++ abstractions \"zero-cost\"\n\n";
    
    std::cout << "When SROA applies:\n";
    std::cout << "- Small structs used locally\n";
    std::cout << "- No address taken (&object)\n";
    std::cout << "- No pointers/references passed around\n";
    std::cout << "- All uses visible to the compiler\n\n";
    
    std::cout << "When SROA may NOT apply:\n";
    std::cout << "- Address of struct taken\n";
    std::cout << "- Struct passed by pointer/reference to non-inlined function\n";
    std::cout << "- Very large structs\n";
    std::cout << "- Struct stored in containers\n\n";
    
    std::cout << "To verify SROA in assembly:\n";
    std::cout << "  g++ -S -O3 -std=c++17 03_sroa_register_promotion.cpp\n";
    std::cout << "  Look for: no stack allocation, values in registers\n";
    std::cout << "  Or use: https://godbolt.org (Compiler Explorer)\n";
    
    return 0;
}
