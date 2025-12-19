/**
 * Return Value Optimization (RVO) and Named RVO (NRVO)
 * 
 * This code demonstrates how modern compilers optimize away copy/move
 * operations when returning objects from functions.
 * 
 * Key concepts:
 * - RVO: Unnamed return value optimization (guaranteed in C++17)
 * - NRVO: Named return value optimization (not guaranteed but common)
 * - Both eliminate copy/move by constructing directly in destination
 * - std::move() can DISABLE these optimizations!
 * 
 * When RVO/NRVO applies, the compiler:
 * - Allocates space in caller's stack frame
 * - Passes hidden pointer to callee
 * - Callee constructs object directly there
 * - No copy or move needed!
 * 
 * Compile: g++ -O3 -std=c++17 -o 03_rvo_nrvo 03_rvo_nrvo.cpp
 * Run: ./03_rvo_nrvo
 * 
 * To see without RVO: g++ -fno-elide-constructors -O0 -std=c++17 ...
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
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

/**
 * A class that tracks all construction/destruction operations
 */
class TrackedObject {
    static int counter;
    int id;
    char* data;
    size_t size;
    
public:
    explicit TrackedObject(size_t sz = 1024) : size(sz) {
        id = ++counter;
        data = new char[size];
        std::memset(data, 0, size);
        std::cout << "  [" << id << "] Constructor\n";
    }
    
    TrackedObject(const TrackedObject& other) : size(other.size) {
        id = ++counter;
        data = new char[size];
        std::memcpy(data, other.data, size);
        std::cout << "  [" << id << "] Copy constructor (from " << other.id << ")\n";
    }
    
    TrackedObject(TrackedObject&& other) noexcept : size(other.size) {
        id = ++counter;
        data = other.data;
        other.data = nullptr;
        other.size = 0;
        std::cout << "  [" << id << "] Move constructor (from " << other.id << ")\n";
    }
    
    TrackedObject& operator=(const TrackedObject& other) {
        std::cout << "  [" << id << "] Copy assignment (from " << other.id << ")\n";
        if (this != &other) {
            delete[] data;
            size = other.size;
            data = new char[size];
            std::memcpy(data, other.data, size);
        }
        return *this;
    }
    
    TrackedObject& operator=(TrackedObject&& other) noexcept {
        std::cout << "  [" << id << "] Move assignment (from " << other.id << ")\n";
        if (this != &other) {
            delete[] data;
            data = other.data;
            size = other.size;
            other.data = nullptr;
            other.size = 0;
        }
        return *this;
    }
    
    ~TrackedObject() {
        std::cout << "  [" << id << "] Destructor\n";
        delete[] data;
    }
    
    int get_id() const { return id; }
    static void reset_counter() { counter = 0; }
};

int TrackedObject::counter = 0;

// ============================================
// DIFFERENT RETURN PATTERNS
// ============================================

/**
 * Pattern 1: RVO - Return unnamed temporary
 * 
 * This is GUARANTEED to be optimized in C++17.
 * The temporary is constructed directly in the caller's space.
 */
TrackedObject create_rvo() {
    std::cout << "  Creating object with RVO pattern\n";
    return TrackedObject(1024);  // Unnamed temporary
}

/**
 * Pattern 2: NRVO - Return named local variable
 * 
 * This is NOT guaranteed but most compilers do it.
 * The named object is constructed directly in the caller's space.
 */
TrackedObject create_nrvo() {
    std::cout << "  Creating object with NRVO pattern\n";
    TrackedObject obj(1024);  // Named object
    return obj;               // Return named object
}

/**
 * Pattern 3: Conditional return - NRVO may NOT apply
 * 
 * When there are multiple return paths with different objects,
 * NRVO cannot apply because compiler doesn't know which one
 * will be returned.
 */
TrackedObject create_conditional(bool flag) {
    std::cout << "  Creating object with conditional pattern\n";
    TrackedObject obj1(1024);
    TrackedObject obj2(2048);
    
    if (flag) {
        return obj1;  // Which one? Compiler can't know at compile time
    } else {
        return obj2;  // So it can't place either in caller's space
    }
}

/**
 * Pattern 4: WRONG - Using std::move on return PREVENTS NRVO!
 * 
 * std::move forces a move instead of allowing copy elision.
 * This is a common mistake!
 */
TrackedObject create_wrong_move() {
    std::cout << "  Creating object with WRONG std::move pattern\n";
    TrackedObject obj(1024);
    return std::move(obj);  // BAD! Prevents NRVO, forces move
}

/**
 * Pattern 5: Return by parameter - old style, avoid
 */
void create_out_param(TrackedObject& out) {
    std::cout << "  Creating object with out-parameter pattern\n";
    out = TrackedObject(1024);  // Temporary + move assignment
}

/**
 * Pattern 6: Multiple operations, single return - NRVO OK
 */
TrackedObject create_modified() {
    std::cout << "  Creating and modifying object\n";
    TrackedObject obj(1024);
    // ... do some operations on obj ...
    return obj;  // NRVO can still apply
}

/**
 * Pattern 7: Return from nested scope - NRVO usually works
 */
TrackedObject create_from_scope(bool flag) {
    std::cout << "  Creating object from nested scope\n";
    if (flag) {
        TrackedObject obj(1024);
        // ... do something ...
        return obj;  // NRVO can apply (single path)
    }
    // Return different object if flag is false
    TrackedObject obj(2048);
    return obj;  // NRVO might not apply due to multiple paths
}

// ============================================
// PERFORMANCE TESTS
// ============================================

class LargeObject {
    std::vector<int> data;
public:
    explicit LargeObject(size_t size) : data(size, 42) {}
    size_t size() const { return data.size(); }
};

// RVO version
LargeObject create_large_rvo(size_t size) {
    return LargeObject(size);
}

// NRVO version
LargeObject create_large_nrvo(size_t size) {
    LargeObject obj(size);
    return obj;
}

// Wrong move version
LargeObject create_large_wrong(size_t size) {
    LargeObject obj(size);
    return std::move(obj);  // Prevents RVO!
}

// Out parameter version
void create_large_out(size_t size, LargeObject& out) {
    out = LargeObject(size);
}

volatile size_t sink;

int main() {
    std::cout << "=== RVO and NRVO Demo ===\n\n";
    
    // ==========================================
    // Demo 1: Different return patterns
    // ==========================================
    std::cout << "=== Pattern Comparison ===\n\n";
    
    std::cout << "Pattern 1: RVO (unnamed return)\n";
    TrackedObject::reset_counter();
    {
        TrackedObject obj = create_rvo();
        std::cout << "  Got object with id: " << obj.get_id() << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Pattern 2: NRVO (named return)\n";
    TrackedObject::reset_counter();
    {
        TrackedObject obj = create_nrvo();
        std::cout << "  Got object with id: " << obj.get_id() << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Pattern 3: Conditional return (NRVO may fail)\n";
    TrackedObject::reset_counter();
    {
        TrackedObject obj = create_conditional(true);
        std::cout << "  Got object with id: " << obj.get_id() << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Pattern 4: WRONG - std::move on return\n";
    TrackedObject::reset_counter();
    {
        TrackedObject obj = create_wrong_move();
        std::cout << "  Got object with id: " << obj.get_id() << "\n";
    }
    std::cout << "\n";
    
    std::cout << "Pattern 5: Out parameter (old style)\n";
    TrackedObject::reset_counter();
    {
        TrackedObject obj(0);  // Default construct first
        create_out_param(obj);
        std::cout << "  Got object with id: " << obj.get_id() << "\n";
    }
    std::cout << "\n";
    
    // ==========================================
    // Demo 2: Performance comparison
    // ==========================================
    std::cout << "=== Performance Comparison ===\n\n";
    
    const size_t object_size = 1000000;  // 1M integers = 4MB
    const int iterations = 1000;
    
    std::cout << "Object size: " << object_size * sizeof(int) / 1024 / 1024 << " MB\n";
    std::cout << "Iterations: " << iterations << "\n\n";
    
    // RVO
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            LargeObject obj = create_large_rvo(object_size);
            sink = obj.size();
        }
        std::cout << "RVO pattern: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    // NRVO
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            LargeObject obj = create_large_nrvo(object_size);
            sink = obj.size();
        }
        std::cout << "NRVO pattern: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    // Wrong move
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            LargeObject obj = create_large_wrong(object_size);
            sink = obj.size();
        }
        std::cout << "std::move (WRONG): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- Prevented optimization!\n";
    }
    
    // Out parameter
    {
        Timer timer;
        LargeObject obj(0);
        for (int i = 0; i < iterations; ++i) {
            create_large_out(object_size, obj);
            sink = obj.size();
        }
        std::cout << "Out parameter: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    std::cout << "\n";
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "RVO/NRVO - How it works:\n\n";
    std::cout << "Without RVO:           With RVO:\n";
    std::cout << "+-----------+          +-----------+\n";
    std::cout << "| Caller    |          | Caller    |\n";
    std::cout << "| +-------+ |          | +-------+ |\n";
    std::cout << "| | dest  |<---copy/-- | | dest  |<-+\n";
    std::cout << "| +-------+ |   move   | +-------+ |\n";
    std::cout << "+-----------+          +-----------+\n";
    std::cout << "+-----------+                |\n";
    std::cout << "| Callee    |          +-----------+\n";
    std::cout << "| +-------+ |          | Callee    |\n";
    std::cout << "| | temp  |----+       |  (no      |\n";
    std::cout << "| +-------+ |          |   temp!)  |\n";
    std::cout << "+-----------+          +-----------+\n\n";
    
    std::cout << "Rules:\n\n";
    
    std::cout << "1. PREFER returning by value:\n";
    std::cout << "   Widget create() { return Widget(); }  // Good!\n\n";
    
    std::cout << "2. DON'T use std::move on return:\n";
    std::cout << "   // BAD - prevents RVO/NRVO!\n";
    std::cout << "   Widget create() {\n";
    std::cout << "       Widget w;\n";
    std::cout << "       return std::move(w);  // Wrong!\n";
    std::cout << "   }\n";
    std::cout << "   // GOOD\n";
    std::cout << "   Widget create() {\n";
    std::cout << "       Widget w;\n";
    std::cout << "       return w;  // Let compiler optimize\n";
    std::cout << "   }\n\n";
    
    std::cout << "3. SINGLE return path for NRVO:\n";
    std::cout << "   // NRVO may not apply\n";
    std::cout << "   Widget create(bool b) {\n";
    std::cout << "       Widget a, b;\n";
    std::cout << "       return b ? a : b;  // Which one?\n";
    std::cout << "   }\n\n";
    
    std::cout << "4. C++17 guarantees:\n";
    std::cout << "   - RVO for temporaries is mandatory\n";
    std::cout << "   - NRVO still optional (but common)\n\n";
    
    std::cout << "5. DON'T use out parameters:\n";
    std::cout << "   // Old style - avoid\n";
    std::cout << "   void create(Widget& out);\n";
    std::cout << "   // Modern style - prefer\n";
    std::cout << "   Widget create();\n";
    
    return 0;
}
