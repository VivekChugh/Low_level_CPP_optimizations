/**
 * Movable Type with Efficient Move Constructor
 * 
 * This code demonstrates how to implement a type with proper move semantics
 * to avoid expensive copies.
 * 
 * Key concepts:
 * - Move semantics transfer ownership instead of copying
 * - Move constructor takes an rvalue reference (T&&)
 * - After move, source object is in valid but unspecified state
 * - std::move() casts lvalue to rvalue reference
 * - noexcept enables important optimizations (vector reallocation)
 * 
 * Compile: g++ -O3 -std=c++17 -o 01_movable_type 01_movable_type.cpp
 * Run: ./01_movable_type
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <utility>

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

// Counters for tracking operations
struct OperationCounters {
    static int default_constructions;
    static int copy_constructions;
    static int move_constructions;
    static int copy_assignments;
    static int move_assignments;
    static int destructions;
    
    static void reset() {
        default_constructions = 0;
        copy_constructions = 0;
        move_constructions = 0;
        copy_assignments = 0;
        move_assignments = 0;
        destructions = 0;
    }
    
    static void print(const std::string& label) {
        std::cout << label << ":\n";
        std::cout << "  Default constructions: " << default_constructions << "\n";
        std::cout << "  Copy constructions: " << copy_constructions << "\n";
        std::cout << "  Move constructions: " << move_constructions << "\n";
        std::cout << "  Copy assignments: " << copy_assignments << "\n";
        std::cout << "  Move assignments: " << move_assignments << "\n";
        std::cout << "  Destructions: " << destructions << "\n\n";
    }
};

int OperationCounters::default_constructions = 0;
int OperationCounters::copy_constructions = 0;
int OperationCounters::move_constructions = 0;
int OperationCounters::copy_assignments = 0;
int OperationCounters::move_assignments = 0;
int OperationCounters::destructions = 0;

/**
 * A type that manages a heap-allocated buffer
 * Demonstrates proper move semantics implementation
 */
class Buffer {
    char* data_;
    size_t size_;
    
public:
    /**
     * Default constructor
     */
    Buffer() : data_(nullptr), size_(0) {
        ++OperationCounters::default_constructions;
    }
    
    /**
     * Constructor - allocates buffer of given size
     */
    explicit Buffer(size_t size) : data_(new char[size]), size_(size) {
        ++OperationCounters::default_constructions;
        // Initialize to avoid undefined behavior
        std::memset(data_, 0, size_);
    }
    
    /**
     * Copy constructor - EXPENSIVE
     * 
     * Must allocate new memory and copy all data.
     * Time complexity: O(n) where n is buffer size.
     */
    Buffer(const Buffer& other) : data_(nullptr), size_(other.size_) {
        ++OperationCounters::copy_constructions;
        
        if (other.data_) {
            // Allocate new buffer
            data_ = new char[size_];
            // Copy all bytes
            std::memcpy(data_, other.data_, size_);
        }
    }
    
    /**
     * Move constructor - CHEAP
     * 
     * Just transfers pointer ownership, no allocation or copy.
     * Time complexity: O(1).
     * 
     * The noexcept specifier is CRITICAL:
     * - std::vector uses move only if it's noexcept
     * - Otherwise falls back to copy for exception safety
     */
    Buffer(Buffer&& other) noexcept 
        : data_(other.data_), size_(other.size_) {
        ++OperationCounters::move_constructions;
        
        // Leave source in valid but empty state
        other.data_ = nullptr;
        other.size_ = 0;
    }
    
    /**
     * Copy assignment - EXPENSIVE
     */
    Buffer& operator=(const Buffer& other) {
        ++OperationCounters::copy_assignments;
        
        if (this != &other) {
            // Clean up existing data
            delete[] data_;
            
            // Copy from other
            size_ = other.size_;
            if (other.data_) {
                data_ = new char[size_];
                std::memcpy(data_, other.data_, size_);
            } else {
                data_ = nullptr;
            }
        }
        return *this;
    }
    
    /**
     * Move assignment - CHEAP
     * 
     * Swaps ownership, no allocation or copy.
     * Must be noexcept for optimal vector performance.
     */
    Buffer& operator=(Buffer&& other) noexcept {
        ++OperationCounters::move_assignments;
        
        if (this != &other) {
            // Clean up existing data
            delete[] data_;
            
            // Steal from other
            data_ = other.data_;
            size_ = other.size_;
            
            // Leave other in valid empty state
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    
    /**
     * Destructor
     */
    ~Buffer() {
        ++OperationCounters::destructions;
        delete[] data_;
    }
    
    // Getters
    size_t size() const { return size_; }
    const char* data() const { return data_; }
};

/**
 * A type WITHOUT move semantics (copy only)
 * For comparison
 */
class BufferCopyOnly {
    char* data_;
    size_t size_;
    
public:
    BufferCopyOnly() : data_(nullptr), size_(0) {
        ++OperationCounters::default_constructions;
    }
    
    explicit BufferCopyOnly(size_t size) : data_(new char[size]), size_(size) {
        ++OperationCounters::default_constructions;
        std::memset(data_, 0, size_);
    }
    
    // Copy constructor
    BufferCopyOnly(const BufferCopyOnly& other) : data_(nullptr), size_(other.size_) {
        ++OperationCounters::copy_constructions;
        if (other.data_) {
            data_ = new char[size_];
            std::memcpy(data_, other.data_, size_);
        }
    }
    
    // Copy assignment
    BufferCopyOnly& operator=(const BufferCopyOnly& other) {
        ++OperationCounters::copy_assignments;
        if (this != &other) {
            delete[] data_;
            size_ = other.size_;
            if (other.data_) {
                data_ = new char[size_];
                std::memcpy(data_, other.data_, size_);
            } else {
                data_ = nullptr;
            }
        }
        return *this;
    }
    
    // NO move constructor or move assignment
    // Compiler will use copy instead
    
    ~BufferCopyOnly() {
        ++OperationCounters::destructions;
        delete[] data_;
    }
    
    size_t size() const { return size_; }
};

/**
 * Factory function that returns by value
 * 
 * With move semantics, this is efficient because the returned
 * object can be moved into the destination.
 */
Buffer create_buffer(size_t size) {
    Buffer buf(size);
    // Fill with some data
    return buf;  // Move (or RVO) happens here
}

BufferCopyOnly create_buffer_copy_only(size_t size) {
    BufferCopyOnly buf(size);
    return buf;  // Copy (or RVO) happens here
}

int main() {
    std::cout << "=== Move Semantics Demo ===\n\n";
    
    const size_t buffer_size = 1024 * 1024;  // 1 MB buffers
    const int num_buffers = 100;
    
    std::cout << "Buffer size: " << buffer_size / 1024 << " KB\n";
    std::cout << "Number of buffers: " << num_buffers << "\n\n";
    
    // ==========================================
    // Test 1: Vector with move-enabled type
    // ==========================================
    std::cout << "=== Test 1: vector<Buffer> (Move-Enabled) ===\n";
    OperationCounters::reset();
    {
        Timer timer;
        std::vector<Buffer> buffers;
        
        for (int i = 0; i < num_buffers; ++i) {
            // push_back with temporary triggers move
            buffers.push_back(Buffer(buffer_size));
        }
        
        std::cout << "Time: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    OperationCounters::print("Operations");
    
    // ==========================================
    // Test 2: Vector with copy-only type
    // ==========================================
    std::cout << "=== Test 2: vector<BufferCopyOnly> (Copy-Only) ===\n";
    OperationCounters::reset();
    {
        Timer timer;
        std::vector<BufferCopyOnly> buffers;
        
        for (int i = 0; i < num_buffers; ++i) {
            buffers.push_back(BufferCopyOnly(buffer_size));
        }
        
        std::cout << "Time: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    OperationCounters::print("Operations");
    
    // ==========================================
    // Test 3: Vector with reserve (no reallocation)
    // ==========================================
    std::cout << "=== Test 3: vector with reserve() ===\n";
    OperationCounters::reset();
    {
        Timer timer;
        std::vector<Buffer> buffers;
        buffers.reserve(num_buffers);  // Pre-allocate
        
        for (int i = 0; i < num_buffers; ++i) {
            buffers.push_back(Buffer(buffer_size));
        }
        
        std::cout << "Time: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    OperationCounters::print("Operations");
    
    // ==========================================
    // Test 4: emplace_back (construct in place)
    // ==========================================
    std::cout << "=== Test 4: emplace_back (In-Place Construction) ===\n";
    OperationCounters::reset();
    {
        Timer timer;
        std::vector<Buffer> buffers;
        buffers.reserve(num_buffers);
        
        for (int i = 0; i < num_buffers; ++i) {
            // Constructs directly in vector, no temporary
            buffers.emplace_back(buffer_size);
        }
        
        std::cout << "Time: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    OperationCounters::print("Operations");
    
    // ==========================================
    // Test 5: Factory function return
    // ==========================================
    std::cout << "=== Test 5: Factory Function Return ===\n";
    std::cout << "(Demonstrating RVO/move on return)\n";
    OperationCounters::reset();
    {
        Timer timer;
        std::vector<Buffer> buffers;
        buffers.reserve(num_buffers);
        
        for (int i = 0; i < num_buffers; ++i) {
            // Factory returns by value - may use RVO or move
            buffers.push_back(create_buffer(buffer_size));
        }
        
        std::cout << "Time: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    OperationCounters::print("Operations");
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Move Semantics Benefits:\n\n";
    
    std::cout << "1. COPY vs MOVE:\n";
    std::cout << "   - Copy: Allocate + memcpy = O(n)\n";
    std::cout << "   - Move: Transfer pointer = O(1)\n\n";
    
    std::cout << "2. NOEXCEPT IS CRITICAL:\n";
    std::cout << "   - std::vector needs noexcept move\n";
    std::cout << "   - Without it, vector falls back to copy\n";
    std::cout << "   - For exception safety during reallocation\n\n";
    
    std::cout << "3. RULE OF FIVE:\n";
    std::cout << "   If you define any of:\n";
    std::cout << "   - Destructor\n";
    std::cout << "   - Copy constructor\n";
    std::cout << "   - Copy assignment\n";
    std::cout << "   - Move constructor\n";
    std::cout << "   - Move assignment\n";
    std::cout << "   You should define ALL of them.\n\n";
    
    std::cout << "4. PREFER emplace_back:\n";
    std::cout << "   - Constructs in place\n";
    std::cout << "   - Avoids temporary + move\n";
    std::cout << "   - Most efficient for containers\n\n";
    
    std::cout << "5. USE reserve():\n";
    std::cout << "   - Avoids vector reallocations\n";
    std::cout << "   - Each reallocation moves all elements\n";
    
    return 0;
}
