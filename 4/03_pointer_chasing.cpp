/**
 * Pointer Chasing Overhead - Contiguous vs. Non-Contiguous Data
 * 
 * This code demonstrates the severe performance penalty of "pointer chasing" -
 * where accessing the next element requires following a pointer to an unknown
 * memory location.
 * 
 * Key concepts:
 * - Contiguous storage (vector): Elements are adjacent in memory
 * - Non-contiguous storage (list): Elements are scattered across the heap
 * - Pointer chasing defeats CPU prefetchers
 * - Each pointer follow is a potential cache miss (~100+ cycles)
 * - Linked data structures have hidden performance costs
 * 
 * This demo compares:
 * 1. std::vector<T>: Contiguous, cache-friendly, prefetcher-friendly
 * 2. std::list<T>: Non-contiguous, cache-unfriendly, pointer chasing
 * 3. Custom linked list: Even worse due to manual allocation
 * 
 * Compile: g++ -O3 -std=c++17 -o 03_pointer_chasing 03_pointer_chasing.cpp
 * Run: ./03_pointer_chasing
 */

#include <iostream>
#include <vector>
#include <list>
#include <chrono>
#include <iomanip>
#include <random>
#include <memory>
#include <algorithm>  // for std::shuffle

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
 * A simple data structure to store in containers
 */
struct Data {
    long long value;
    int id;
    char padding[52];  // Pad to 64 bytes (1 cache line) to emphasize effects
    
    Data() : value(0), id(0) {}
    Data(long long v, int i) : value(v), id(i) {}
};

/**
 * Custom linked list node for manual pointer chasing
 */
struct LinkedNode {
    Data data;
    LinkedNode* next;
    
    LinkedNode(long long v, int i) : data(v, i), next(nullptr) {}
};

/**
 * Sum values in a vector (contiguous memory)
 * 
 * Memory layout:
 * [Data0][Data1][Data2][Data3]...
 * 
 * Access pattern:
 * - CPU loads cache line containing Data0
 * - Data1, Data2, etc. may be in same or adjacent cache lines
 * - Prefetcher detects sequential pattern, loads ahead
 * - Most accesses are cache hits
 */
long long sum_vector(const std::vector<Data>& vec) {
    long long sum = 0;
    
    // Sequential iteration through contiguous memory
    for (const auto& item : vec) {
        sum += item.value;
        // Next element is at predictable location: &item + sizeof(Data)
    }
    
    return sum;
}

/**
 * Sum values in a std::list (non-contiguous memory)
 * 
 * Memory layout (conceptual):
 * [Node0] ----> [Node5] ----> [Node2] ----> [Node7]...
 *   (scattered across heap due to individual allocations)
 * 
 * Access pattern:
 * - Load Node0, follow 'next' pointer
 * - Next node could be ANYWHERE in memory
 * - Prefetcher cannot predict where to load
 * - Each access likely a cache miss
 */
long long sum_list(const std::list<Data>& lst) {
    long long sum = 0;
    
    // Each ++it follows a pointer to unknown location
    for (const auto& item : lst) {
        sum += item.value;
        // Next element requires following a pointer - cache miss!
    }
    
    return sum;
}

/**
 * Sum values in custom linked list (worst case)
 * 
 * Even worse than std::list because:
 * - Nodes allocated one at a time (maximum fragmentation)
 * - No allocator optimizations
 */
long long sum_linked(LinkedNode* head) {
    long long sum = 0;
    
    LinkedNode* current = head;
    while (current != nullptr) {
        sum += current->data.value;
        current = current->next;  // Pointer chase - cache miss!
    }
    
    return sum;
}

/**
 * Vector of pointers (another form of pointer chasing)
 * 
 * The vector itself is contiguous, but the data it points to is scattered.
 * This is common in OOP designs with virtual inheritance.
 */
long long sum_vector_of_pointers(const std::vector<std::unique_ptr<Data>>& vec) {
    long long sum = 0;
    
    for (const auto& ptr : vec) {
        // Vector iteration is cheap, but dereferencing ptr causes cache miss
        sum += ptr->value;
    }
    
    return sum;
}

/**
 * Randomize a linked list's memory layout
 * This simulates real-world fragmentation where nodes are scattered.
 */
LinkedNode* create_fragmented_linked_list(size_t n, std::vector<std::unique_ptr<LinkedNode>>& storage) {
    // Allocate all nodes
    storage.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        storage.push_back(std::make_unique<LinkedNode>(static_cast<long long>(i), static_cast<int>(i)));
    }
    
    // Shuffle the order to simulate fragmentation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; ++i) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), gen);
    
    // Link in shuffled order
    for (size_t i = 0; i < n - 1; ++i) {
        storage[indices[i]]->next = storage[indices[i + 1]].get();
    }
    storage[indices[n - 1]]->next = nullptr;
    
    return storage[indices[0]].get();
}

int main() {
    std::cout << "=== Pointer Chasing Overhead Demo ===\n\n";
    
    // Configuration
    const size_t N = 1000000;  // 1 million elements
    const int num_runs = 5;
    
    std::cout << "Number of elements: " << N << "\n";
    std::cout << "sizeof(Data): " << sizeof(Data) << " bytes\n";
    std::cout << "sizeof(LinkedNode): " << sizeof(LinkedNode) << " bytes\n\n";
    
    // Memory usage
    std::cout << "Memory usage:\n";
    std::cout << "  vector<Data>: " << (N * sizeof(Data)) / (1024.0 * 1024.0) << " MB (contiguous)\n";
    std::cout << "  list<Data>:   ~" << (N * (sizeof(Data) + 2 * sizeof(void*))) / (1024.0 * 1024.0) 
              << " MB (scattered + overhead)\n";
    std::cout << "  LinkedNode:   ~" << (N * sizeof(LinkedNode)) / (1024.0 * 1024.0) 
              << " MB (scattered)\n\n";
    
    // Create vector (contiguous)
    std::cout << "Creating vector (contiguous memory)...\n";
    std::vector<Data> vec;
    vec.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        vec.emplace_back(static_cast<long long>(i), static_cast<int>(i));
    }
    
    // Create list (non-contiguous)
    std::cout << "Creating std::list (non-contiguous)...\n";
    std::list<Data> lst;
    for (size_t i = 0; i < N; ++i) {
        lst.emplace_back(static_cast<long long>(i), static_cast<int>(i));
    }
    
    // Create fragmented linked list
    std::cout << "Creating fragmented linked list...\n";
    std::vector<std::unique_ptr<LinkedNode>> linked_storage;
    LinkedNode* linked_head = create_fragmented_linked_list(N, linked_storage);
    
    // Create vector of pointers
    std::cout << "Creating vector of pointers...\n";
    std::vector<std::unique_ptr<Data>> vec_ptrs;
    vec_ptrs.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        vec_ptrs.push_back(std::make_unique<Data>(static_cast<long long>(i), static_cast<int>(i)));
    }
    // Shuffle to simulate scattered heap allocations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(vec_ptrs.begin(), vec_ptrs.end(), gen);
    
    std::cout << "Data structures created.\n\n";
    
    // Warm-up
    sink = sum_vector(vec);
    sink = sum_list(lst);
    sink = sum_linked(linked_head);
    sink = sum_vector_of_pointers(vec_ptrs);
    
    // Benchmark results storage
    double vec_time = 0, list_time = 0, linked_time = 0, ptr_time = 0;
    long long result;
    
    // Test 1: Vector (contiguous)
    std::cout << "=== Test 1: std::vector<Data> (Contiguous) ===\n";
    {
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = sum_vector(vec);
            vec_time += timer.elapsed_ms();
            sink = result;
        }
        vec_time /= num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) << vec_time << " ms\n";
        std::cout << "Result: " << result << "\n\n";
    }
    
    // Test 2: std::list (non-contiguous)
    std::cout << "=== Test 2: std::list<Data> (Non-Contiguous) ===\n";
    {
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = sum_list(lst);
            list_time += timer.elapsed_ms();
            sink = result;
        }
        list_time /= num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) << list_time << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "Slowdown vs vector: " << std::setprecision(1) << list_time / vec_time << "x\n\n";
    }
    
    // Test 3: Custom linked list (fragmented)
    std::cout << "=== Test 3: Custom Linked List (Fragmented) ===\n";
    {
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = sum_linked(linked_head);
            linked_time += timer.elapsed_ms();
            sink = result;
        }
        linked_time /= num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) << linked_time << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "Slowdown vs vector: " << std::setprecision(1) << linked_time / vec_time << "x\n\n";
    }
    
    // Test 4: Vector of pointers
    std::cout << "=== Test 4: vector<unique_ptr<Data>> (Pointer Chasing) ===\n";
    {
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            result = sum_vector_of_pointers(vec_ptrs);
            ptr_time += timer.elapsed_ms();
            sink = result;
        }
        ptr_time /= num_runs;
        std::cout << "Average time: " << std::fixed << std::setprecision(2) << ptr_time << " ms\n";
        std::cout << "Result: " << result << "\n";
        std::cout << "Slowdown vs vector: " << std::setprecision(1) << ptr_time / vec_time << "x\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Performance comparison:\n";
    std::cout << "  std::vector<Data>:       " << std::fixed << std::setprecision(2) 
              << vec_time << " ms (baseline)\n";
    std::cout << "  std::list<Data>:         " << list_time << " ms (" 
              << std::setprecision(1) << list_time / vec_time << "x slower)\n";
    std::cout << "  Custom linked list:      " << std::setprecision(2) << linked_time << " ms (" 
              << std::setprecision(1) << linked_time / vec_time << "x slower)\n";
    std::cout << "  vector<unique_ptr>:      " << std::setprecision(2) << ptr_time << " ms (" 
              << std::setprecision(1) << ptr_time / vec_time << "x slower)\n\n";
    
    std::cout << "Why is pointer chasing so slow?\n\n";
    
    std::cout << "1. CACHE MISSES:\n";
    std::cout << "   - Vector: Prefetcher loads ahead, most accesses hit cache\n";
    std::cout << "   - List: Each node at random location, almost every access misses\n";
    std::cout << "   - Cache miss penalty: 100-300 CPU cycles\n\n";
    
    std::cout << "2. PREFETCHER FAILURE:\n";
    std::cout << "   - Prefetcher detects patterns like 'address + constant'\n";
    std::cout << "   - Cannot predict where a pointer points\n";
    std::cout << "   - Data arrives too late to be useful\n\n";
    
    std::cout << "3. TLB PRESSURE:\n";
    std::cout << "   - Scattered data touches many memory pages\n";
    std::cout << "   - TLB (Translation Lookaside Buffer) holds limited page mappings\n";
    std::cout << "   - TLB misses add even more latency\n\n";
    
    std::cout << "4. MEMORY BANDWIDTH WASTE:\n";
    std::cout << "   - Load 64-byte cache line for each node\n";
    std::cout << "   - Often only use a small part of each line\n\n";
    
    std::cout << "=== Guidelines ===\n\n";
    
    std::cout << "PREFER: Contiguous data structures\n";
    std::cout << "  - std::vector for most cases\n";
    std::cout << "  - std::array for fixed-size\n";
    std::cout << "  - std::deque if need front insertion\n\n";
    
    std::cout << "AVOID: Pointer-heavy structures for hot paths\n";
    std::cout << "  - std::list (rarely faster than vector)\n";
    std::cout << "  - std::map (use std::unordered_map or sorted vector)\n";
    std::cout << "  - Trees and linked structures in tight loops\n\n";
    
    std::cout << "WHEN POINTERS ARE NECESSARY:\n";
    std::cout << "  - Consider object pools (pre-allocated contiguous memory)\n";
    std::cout << "  - Use indices instead of pointers (smaller, cache-friendlier)\n";
    std::cout << "  - Batch operations to amortize cache miss cost\n";
    
    return 0;
}
