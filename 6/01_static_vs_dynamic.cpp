/**
 * Static vs Dynamic Polymorphism Cost
 * 
 * This code demonstrates the performance difference between:
 * - Virtual functions (dynamic polymorphism, runtime dispatch)
 * - Templates (static polymorphism, compile-time dispatch)
 * 
 * Key concepts:
 * - Virtual functions require indirect call through vtable pointer
 * - Indirect calls are harder for CPU to predict (call target varies)
 * - Templates generate specialized code, enabling direct calls and inlining
 * - Virtual calls can be 2-10x slower in tight loops
 * 
 * When to use each:
 * - Virtual functions: When types are only known at runtime
 * - Templates: When types are known at compile time
 * 
 * Compile: g++ -O3 -std=c++17 -o 01_static_vs_dynamic 01_static_vs_dynamic.cpp
 * Run: ./01_static_vs_dynamic
 */

#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <iomanip>
#include <random>

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
volatile double sink;

// ============================================
// DYNAMIC POLYMORPHISM (Virtual Functions)
// ============================================

/**
 * Abstract base class with virtual function
 * 
 * Each derived class gets a vtable (virtual table) containing
 * pointers to its implementations of virtual functions.
 * 
 * Memory layout of an object:
 * +---------------+
 * | vptr          |  --> Points to vtable
 * | data members  |
 * +---------------+
 */
class ShapeBase {
public:
    virtual ~ShapeBase() = default;
    
    /**
     * Virtual function - resolved at runtime via vtable lookup
     * 
     * When called:
     * 1. Load vptr from object
     * 2. Load function pointer from vtable
     * 3. Call through function pointer (indirect call)
     */
    virtual double area() const = 0;
};

/**
 * Concrete implementation - Circle
 */
class CircleVirtual : public ShapeBase {
    double radius;
public:
    CircleVirtual(double r) : radius(r) {}
    
    // Override - this function's address goes in Circle's vtable
    double area() const override {
        return 3.14159265358979 * radius * radius;
    }
};

/**
 * Concrete implementation - Square
 */
class SquareVirtual : public ShapeBase {
    double side;
public:
    SquareVirtual(double s) : side(s) {}
    
    // Override - this function's address goes in Square's vtable
    double area() const override {
        return side * side;
    }
};

// ============================================
// STATIC POLYMORPHISM (Templates/CRTP)
// ============================================

/**
 * CRTP (Curiously Recurring Template Pattern) base
 * 
 * No virtual functions = no vtable = no indirect calls
 * The derived type is known at compile time via the template parameter
 */
template <typename Derived>
class ShapeCRTP {
public:
    /**
     * Non-virtual - resolved at compile time
     * 
     * The static_cast is safe because Derived must inherit from ShapeCRTP<Derived>
     * This enables compile-time polymorphism without virtual overhead
     */
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

/**
 * Circle using CRTP
 */
class CircleCRTP : public ShapeCRTP<CircleCRTP> {
    double radius;
public:
    CircleCRTP(double r) : radius(r) {}
    
    // Implementation - called directly, can be inlined
    double area_impl() const {
        return 3.14159265358979 * radius * radius;
    }
};

/**
 * Square using CRTP
 */
class SquareCRTP : public ShapeCRTP<SquareCRTP> {
    double side;
public:
    SquareCRTP(double s) : side(s) {}
    
    // Implementation - called directly, can be inlined
    double area_impl() const {
        return side * side;
    }
};

// ============================================
// NO POLYMORPHISM (Direct calls)
// ============================================

/**
 * Plain class - no inheritance, no virtual
 * Serves as baseline for comparison
 */
class CirclePlain {
    double radius;
public:
    CirclePlain(double r) : radius(r) {}
    
    // Direct call - can be fully inlined
    double area() const {
        return 3.14159265358979 * radius * radius;
    }
};

// ============================================
// TEST FUNCTIONS
// ============================================

/**
 * Test with virtual function calls
 * 
 * The loop calls area() through a pointer to base class.
 * Each call goes through the vtable (indirect call).
 */
double sum_areas_virtual(const std::vector<ShapeBase*>& shapes) {
    double total = 0;
    
    for (const auto* shape : shapes) {
        // vtable lookup for each call:
        // 1. Load vptr from *shape
        // 2. Load area() address from vtable
        // 3. Indirect call to area()
        total += shape->area();
    }
    
    return total;
}

/**
 * Test with CRTP - but requires knowing the type
 * 
 * This is a template function that works with any CRTP-derived type.
 * The actual type is known at compile time, enabling inlining.
 */
template <typename ShapeType>
double sum_areas_crtp(const std::vector<ShapeType>& shapes) {
    double total = 0;
    
    for (const auto& shape : shapes) {
        // Direct call - type known at compile time
        // Can be inlined by the compiler
        total += shape.area();
    }
    
    return total;
}

/**
 * Test with plain class (baseline)
 */
double sum_areas_plain(const std::vector<CirclePlain>& circles) {
    double total = 0;
    
    for (const auto& circle : circles) {
        // Direct call, easily inlined
        total += circle.area();
    }
    
    return total;
}

/**
 * Test with virtual functions but only one type (monomorphic)
 * 
 * Even with virtual functions, if only one type is used,
 * modern CPUs can predict the call target effectively.
 */
double sum_areas_virtual_monomorphic(const std::vector<std::unique_ptr<ShapeBase>>& shapes) {
    double total = 0;
    
    for (const auto& shape : shapes) {
        total += shape->area();
    }
    
    return total;
}

int main() {
    std::cout << "=== Static vs Dynamic Polymorphism ===\n\n";
    
    // Configuration
    const size_t N = 10000000;  // 10 million shapes
    const int num_runs = 5;
    
    std::cout << "Number of shapes: " << N << "\n\n";
    
    // Random number generator for shape parameters
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.1, 10.0);
    
    // ==========================================
    // Test 1: Virtual with single type (monomorphic)
    // ==========================================
    std::cout << "=== Test 1: Virtual - Single Type (Monomorphic) ===\n";
    std::cout << "(CPU can predict the call target)\n";
    {
        // All circles
        std::vector<std::unique_ptr<ShapeBase>> shapes;
        shapes.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            shapes.push_back(std::make_unique<CircleVirtual>(dist(gen)));
        }
        
        // Warm-up
        sink = sum_areas_virtual_monomorphic(shapes);
        
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            sink = sum_areas_virtual_monomorphic(shapes);
            total_time += timer.elapsed_ms();
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // ==========================================
    // Test 2: Virtual with mixed types (polymorphic)
    // ==========================================
    std::cout << "=== Test 2: Virtual - Mixed Types (Polymorphic) ===\n";
    std::cout << "(Call target varies - harder to predict)\n";
    {
        // Alternating circles and squares
        std::vector<ShapeBase*> shapes;
        shapes.reserve(N);
        
        // We need to keep the objects alive
        std::vector<CircleVirtual> circles;
        std::vector<SquareVirtual> squares;
        circles.reserve(N / 2);
        squares.reserve(N / 2);
        
        for (size_t i = 0; i < N; ++i) {
            if (i % 2 == 0) {
                circles.emplace_back(dist(gen));
                shapes.push_back(&circles.back());
            } else {
                squares.emplace_back(dist(gen));
                shapes.push_back(&squares.back());
            }
        }
        
        // Warm-up
        sink = sum_areas_virtual(shapes);
        
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            sink = sum_areas_virtual(shapes);
            total_time += timer.elapsed_ms();
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // ==========================================
    // Test 3: CRTP (compile-time polymorphism)
    // ==========================================
    std::cout << "=== Test 3: CRTP (Static Polymorphism) ===\n";
    std::cout << "(Direct calls, can be inlined)\n";
    {
        std::vector<CircleCRTP> circles;
        circles.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            circles.emplace_back(dist(gen));
        }
        
        // Warm-up
        sink = sum_areas_crtp(circles);
        
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            sink = sum_areas_crtp(circles);
            total_time += timer.elapsed_ms();
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // ==========================================
    // Test 4: Plain class (no polymorphism)
    // ==========================================
    std::cout << "=== Test 4: Plain Class (No Polymorphism) ===\n";
    std::cout << "(Baseline - direct calls)\n";
    {
        std::vector<CirclePlain> circles;
        circles.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            circles.emplace_back(dist(gen));
        }
        
        // Warm-up
        sink = sum_areas_plain(circles);
        
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            sink = sum_areas_plain(circles);
            total_time += timer.elapsed_ms();
        }
        
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Virtual Function Overhead:\n\n";
    
    std::cout << "1. VTABLE LOOKUP (every virtual call):\n";
    std::cout << "   - Load vptr from object (memory access)\n";
    std::cout << "   - Load function address from vtable (memory access)\n";
    std::cout << "   - Indirect call (harder to predict)\n\n";
    
    std::cout << "2. MONOMORPHIC CASE (single type):\n";
    std::cout << "   - CPU learns the call target\n";
    std::cout << "   - Can predict well after warmup\n";
    std::cout << "   - Still has vtable lookup overhead\n\n";
    
    std::cout << "3. POLYMORPHIC CASE (mixed types):\n";
    std::cout << "   - Call target varies\n";
    std::cout << "   - Harder for CPU to predict\n";
    std::cout << "   - More branch mispredictions\n\n";
    
    std::cout << "4. STATIC POLYMORPHISM (CRTP/Templates):\n";
    std::cout << "   - No vtable, no indirect calls\n";
    std::cout << "   - Compiler can inline everything\n";
    std::cout << "   - Type must be known at compile time\n\n";
    
    std::cout << "When to prefer templates over virtual:\n";
    std::cout << "- Hot loops with many calls to the same interface\n";
    std::cout << "- Types known at compile time\n";
    std::cout << "- Performance-critical code paths\n";
    
    return 0;
}
