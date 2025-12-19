/**
 * string_view to Avoid String Copies
 * 
 * This code demonstrates how std::string_view can eliminate unnecessary
 * string copies when you only need to read string data.
 * 
 * Key concepts:
 * - std::string_view is a non-owning view of string data
 * - No memory allocation, just pointer + length
 * - Can view std::string, const char*, or substring
 * - Use when function only needs to READ string data
 * - Never store string_view if the underlying data might go away
 * 
 * Compile: g++ -O3 -std=c++17 -o 02_string_view 02_string_view.cpp
 * Run: ./02_string_view
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <algorithm>

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
volatile size_t sink;

// ============================================
// FUNCTIONS THAT TAKE STRINGS BY DIFFERENT METHODS
// ============================================

/**
 * Version 1: Take by const std::string&
 * 
 * If caller has std::string: No copy (good)
 * If caller has const char*: Temporary std::string created (bad)
 * If caller wants substring: Must create new string (bad)
 */
size_t count_vowels_string_ref(const std::string& str) {
    size_t count = 0;
    for (char c : str) {
        char lower = std::tolower(c);
        if (lower == 'a' || lower == 'e' || lower == 'i' || 
            lower == 'o' || lower == 'u') {
            ++count;
        }
    }
    return count;
}

/**
 * Version 2: Take by std::string_view
 * 
 * If caller has std::string: No copy, direct view
 * If caller has const char*: No copy, direct view
 * If caller wants substring: No copy, just adjust view
 * 
 * string_view is just: const char* data + size_t length
 * Total size: 16 bytes (on 64-bit), always copied by value
 */
size_t count_vowels_string_view(std::string_view str) {
    size_t count = 0;
    for (char c : str) {
        char lower = std::tolower(c);
        if (lower == 'a' || lower == 'e' || lower == 'i' || 
            lower == 'o' || lower == 'u') {
            ++count;
        }
    }
    return count;
}

/**
 * Version 3: Take by value (worst for large strings)
 * 
 * Always copies the string, even if caller has std::string.
 */
size_t count_vowels_string_value(std::string str) {
    size_t count = 0;
    for (char c : str) {
        char lower = std::tolower(c);
        if (lower == 'a' || lower == 'e' || lower == 'i' || 
            lower == 'o' || lower == 'u') {
            ++count;
        }
    }
    return count;
}

// ============================================
// SUBSTRING OPERATIONS
// ============================================

/**
 * Extract substring using std::string::substr
 * Creates a new string (allocation)
 */
std::string get_substring_string(const std::string& str, size_t pos, size_t len) {
    return str.substr(pos, len);  // Allocates new string
}

/**
 * Extract substring view using std::string_view::substr
 * No allocation, just returns a new view
 */
std::string_view get_substring_view(std::string_view str, size_t pos, size_t len) {
    return str.substr(pos, len);  // No allocation, O(1)
}

// ============================================
// STRING PARSING EXAMPLES
// ============================================

/**
 * Parse comma-separated values using std::string
 * Each token requires allocation
 */
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));  // Allocation!
        start = end + 1;
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start));  // Allocation!
    
    return tokens;
}

/**
 * Parse comma-separated values using std::string_view
 * No allocations for tokens
 */
std::vector<std::string_view> split_string_view(std::string_view str, char delimiter) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string_view::npos) {
        tokens.push_back(str.substr(start, end - start));  // No allocation!
        start = end + 1;
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start));  // No allocation!
    
    return tokens;
}

/**
 * Check if string starts with prefix
 */
bool starts_with_string(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) return false;
    return str.substr(0, prefix.length()) == prefix;  // Creates substring!
}

bool starts_with_view(std::string_view str, std::string_view prefix) {
    if (str.length() < prefix.length()) return false;
    return str.substr(0, prefix.length()) == prefix;  // No allocation!
}

// Or use C++20's starts_with directly:
// return str.starts_with(prefix);

// ============================================
// GENERATE TEST DATA
// ============================================

std::string generate_random_string(size_t length) {
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789 ";
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(gen)];
    }
    
    return result;
}

int main() {
    std::cout << "=== string_view Demo ===\n\n";
    
    // Size comparison
    std::cout << "=== Size Comparison ===\n";
    std::cout << "sizeof(std::string): " << sizeof(std::string) << " bytes\n";
    std::cout << "sizeof(std::string_view): " << sizeof(std::string_view) << " bytes\n";
    std::cout << "sizeof(const char*): " << sizeof(const char*) << " bytes\n\n";
    
    // Configuration
    const size_t string_length = 10000;
    const int iterations = 100000;
    
    std::cout << "String length: " << string_length << " characters\n";
    std::cout << "Iterations: " << iterations << "\n\n";
    
    // Generate test string
    std::string test_string = generate_random_string(string_length);
    const char* test_cstring = test_string.c_str();
    
    // ==========================================
    // Test 1: Function call with std::string
    // ==========================================
    std::cout << "=== Test 1: Passing std::string to Functions ===\n";
    
    // 1a: const std::string& (from string)
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            sink = count_vowels_string_ref(test_string);
        }
        std::cout << "const string& (from string): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    // 1b: const std::string& (from const char*)
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            // Creates temporary std::string from const char*!
            sink = count_vowels_string_ref(test_cstring);
        }
        std::cout << "const string& (from char*): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- Creates temp string each call!\n";
    }
    
    // 1c: string_view (from string)
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            sink = count_vowels_string_view(test_string);
        }
        std::cout << "string_view (from string): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms\n";
    }
    
    // 1d: string_view (from const char*)
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            sink = count_vowels_string_view(test_cstring);
        }
        std::cout << "string_view (from char*): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- No temp string!\n\n";
    }
    
    // ==========================================
    // Test 2: Substring operations
    // ==========================================
    std::cout << "=== Test 2: Substring Operations ===\n";
    
    // 2a: std::string::substr
    {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            std::string sub = test_string.substr(100, 500);
            sink = sub.length();
        }
        std::cout << "string::substr(): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- Allocates each time!\n";
    }
    
    // 2b: std::string_view::substr
    {
        std::string_view sv = test_string;
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            std::string_view sub = sv.substr(100, 500);
            sink = sub.length();
        }
        std::cout << "string_view::substr(): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- Zero allocation!\n\n";
    }
    
    // ==========================================
    // Test 3: String splitting/parsing
    // ==========================================
    std::cout << "=== Test 3: CSV Parsing ===\n";
    
    // Create CSV-like string
    std::string csv_string;
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) csv_string += ',';
        csv_string += "item" + std::to_string(i);
    }
    
    std::cout << "CSV string with 1000 items\n";
    
    // 3a: split with std::string
    {
        Timer timer;
        for (int i = 0; i < 1000; ++i) {
            auto tokens = split_string(csv_string, ',');
            sink = tokens.size();
        }
        std::cout << "split_string: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- 1000 allocations per split!\n";
    }
    
    // 3b: split with string_view
    {
        Timer timer;
        for (int i = 0; i < 1000; ++i) {
            auto tokens = split_string_view(csv_string, ',');
            sink = tokens.size();
        }
        std::cout << "split_string_view: " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms  <-- Zero string allocations!\n\n";
    }
    
    // ==========================================
    // Test 4: starts_with comparison
    // ==========================================
    std::cout << "=== Test 4: Prefix Checking ===\n";
    
    std::string prefix = "abcdefghij";
    std::string text_with_prefix = prefix + generate_random_string(1000);
    
    // 4a: Using substr comparison
    {
        Timer timer;
        bool result;
        for (int i = 0; i < iterations; ++i) {
            result = starts_with_string(text_with_prefix, prefix);
        }
        std::cout << "starts_with (string): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms, result: " << result << "\n";
    }
    
    // 4b: Using string_view
    {
        Timer timer;
        bool result;
        for (int i = 0; i < iterations; ++i) {
            result = starts_with_view(text_with_prefix, prefix);
        }
        std::cout << "starts_with (view): " << std::fixed << std::setprecision(2)
                  << timer.elapsed_ms() << " ms, result: " << result << "\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "When to use std::string_view:\n\n";
    
    std::cout << "1. FUNCTION PARAMETERS (read-only):\n";
    std::cout << "   void process(std::string_view sv);\n";
    std::cout << "   - Works with string, const char*, substring\n";
    std::cout << "   - No temporary objects created\n\n";
    
    std::cout << "2. SUBSTRING OPERATIONS:\n";
    std::cout << "   auto sub = sv.substr(pos, len);  // O(1), no allocation\n";
    std::cout << "   vs\n";
    std::cout << "   auto sub = str.substr(pos, len); // O(n), allocation\n\n";
    
    std::cout << "3. PARSING/TOKENIZING:\n";
    std::cout << "   - Split into views, not copies\n";
    std::cout << "   - Process views directly\n";
    std::cout << "   - Only materialize to string when needed\n\n";
    
    std::cout << "DANGER ZONES (do NOT use string_view):\n\n";
    
    std::cout << "1. RETURNING LOCAL DATA:\n";
    std::cout << "   // BAD: Returns view of destroyed temp!\n";
    std::cout << "   string_view bad() {\n";
    std::cout << "       string s = \"hello\";\n";
    std::cout << "       return s;  // s destroyed, view dangles!\n";
    std::cout << "   }\n\n";
    
    std::cout << "2. STORING FOR LATER USE:\n";
    std::cout << "   // BAD: string might be modified/destroyed\n";
    std::cout << "   class Bad {\n";
    std::cout << "       string_view sv;  // Danger!\n";
    std::cout << "   };\n\n";
    
    std::cout << "3. WHEN YOU NEED OWNERSHIP:\n";
    std::cout << "   - Store in container for later: use string\n";
    std::cout << "   - Modify the string: use string\n";
    std::cout << "   - Lifetime unclear: use string\n";
    
    return 0;
}
