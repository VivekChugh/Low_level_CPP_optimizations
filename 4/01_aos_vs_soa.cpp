/**
 * Array of Structures (AoS) vs. Structure of Arrays (SoA)
 * 
 * This code demonstrates the performance difference between two common
 * data layout strategies: AoS and SoA.
 * 
 * Key concepts:
 * - AoS: Each object is a struct with all its fields together
 * - SoA: Each field type has its own separate array
 * - When accessing only some fields, SoA avoids loading unused data
 * - SoA enables better SIMD vectorization (contiguous same-type data)
 * - AoS is better when accessing all fields of an object together
 * 
 * Example scenario: Particle simulation
 * - Each particle has position (x, y, z) and velocity (vx, vy, vz)
 * - Position update: x += vx * dt (only touches x and vx)
 * - AoS loads all 6 floats per particle, wastes 4
 * - SoA loads only x and vx arrays, perfect cache utilization
 * 
 * Compile: g++ -O3 -std=c++17 -march=native -o 01_aos_vs_soa 01_aos_vs_soa.cpp
 * Run: ./01_aos_vs_soa
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
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
volatile float sink_float;

/**
 * Array of Structures (AoS) layout
 * 
 * Memory layout for 3 particles:
 * [x0][y0][z0][vx0][vy0][vz0] [x1][y1][z1][vx1][vy1][vz1] [x2][y2][z2][vx2][vy2][vz2]
 * |<----- particle 0 ------>| |<----- particle 1 ------>| |<----- particle 2 ------>|
 * 
 * Each particle's data is contiguous.
 * Good for: accessing all fields of one particle
 * Bad for: accessing one field across all particles
 */
struct ParticleAoS {
    float x, y, z;      // Position
    float vx, vy, vz;   // Velocity
};

/**
 * Structure of Arrays (SoA) layout
 * 
 * Memory layout for 3 particles:
 * x:  [x0][x1][x2]
 * y:  [y0][y1][y2]
 * z:  [z0][z1][z2]
 * vx: [vx0][vx1][vx2]
 * vy: [vy0][vy1][vy2]
 * vz: [vz0][vz1][vz2]
 * 
 * Each field type is contiguous.
 * Good for: accessing one field across all particles (vectorization!)
 * Bad for: accessing all fields of one particle (scattered access)
 */
struct ParticlesSoA {
    std::vector<float> x, y, z;      // Positions
    std::vector<float> vx, vy, vz;   // Velocities
    
    void resize(size_t n) {
        x.resize(n); y.resize(n); z.resize(n);
        vx.resize(n); vy.resize(n); vz.resize(n);
    }
    
    size_t size() const { return x.size(); }
};

/**
 * Update positions using AoS layout
 * 
 * Problem: To update x, we load the entire ParticleAoS struct (24 bytes).
 * But we only need x (4 bytes) and vx (4 bytes) = 8 bytes useful.
 * Cache utilization: 8/24 = 33% for position-only update!
 * 
 * Also, the compiler may struggle to vectorize because x values
 * are not contiguous (they're 24 bytes apart).
 */
void update_positions_aos(std::vector<ParticleAoS>& particles, float dt) {
    const size_t n = particles.size();
    
    for (size_t i = 0; i < n; ++i) {
        // Each iteration loads 24 bytes but only uses 8
        // x, y, z, vx, vy, vz are interleaved in memory
        particles[i].x += particles[i].vx * dt;
        particles[i].y += particles[i].vy * dt;
        particles[i].z += particles[i].vz * dt;
    }
}

/**
 * Update positions using SoA layout
 * 
 * Advantage: x values are contiguous, vx values are contiguous.
 * We load exactly what we need - no wasted cache space.
 * Cache utilization: 100% for position-only update!
 * 
 * The compiler can easily vectorize this:
 * - Load 8 x values at once (AVX: 256 bits = 8 floats)
 * - Load 8 vx values at once
 * - Multiply all by dt
 * - Add and store 8 results at once
 */
void update_positions_soa(ParticlesSoA& particles, float dt) {
    const size_t n = particles.size();
    
    // These loops are highly vectorizable
    // Each array is contiguous, perfect for SIMD
    for (size_t i = 0; i < n; ++i) {
        particles.x[i] += particles.vx[i] * dt;
    }
    
    for (size_t i = 0; i < n; ++i) {
        particles.y[i] += particles.vy[i] * dt;
    }
    
    for (size_t i = 0; i < n; ++i) {
        particles.z[i] += particles.vz[i] * dt;
    }
}

/**
 * Alternative SoA update - single loop (may or may not be faster)
 */
void update_positions_soa_single_loop(ParticlesSoA& particles, float dt) {
    const size_t n = particles.size();
    
    for (size_t i = 0; i < n; ++i) {
        particles.x[i] += particles.vx[i] * dt;
        particles.y[i] += particles.vy[i] * dt;
        particles.z[i] += particles.vz[i] * dt;
    }
}

/**
 * Partial field access - only update X position
 * This shows the maximum advantage of SoA
 */
void update_x_only_aos(std::vector<ParticleAoS>& particles, float dt) {
    const size_t n = particles.size();
    
    for (size_t i = 0; i < n; ++i) {
        // Loading 24 bytes, using only 8 bytes (x and vx)
        // Cache efficiency: 33%
        particles[i].x += particles[i].vx * dt;
    }
}

void update_x_only_soa(ParticlesSoA& particles, float dt) {
    const size_t n = particles.size();
    
    for (size_t i = 0; i < n; ++i) {
        // Loading exactly what we need: x and vx
        // Cache efficiency: 100%
        particles.x[i] += particles.vx[i] * dt;
    }
}

/**
 * Full particle access - where AoS might be similar
 * When we need ALL fields, AoS has good locality per object
 */
float compute_kinetic_energy_aos(const std::vector<ParticleAoS>& particles) {
    float total = 0.0f;
    const size_t n = particles.size();
    
    for (size_t i = 0; i < n; ++i) {
        // Using all velocity components - AoS is reasonable here
        float v_squared = particles[i].vx * particles[i].vx +
                         particles[i].vy * particles[i].vy +
                         particles[i].vz * particles[i].vz;
        total += v_squared;
    }
    
    return total * 0.5f;  // Assuming mass = 1
}

float compute_kinetic_energy_soa(const ParticlesSoA& particles) {
    float total = 0.0f;
    const size_t n = particles.size();
    
    for (size_t i = 0; i < n; ++i) {
        // Accessing three separate arrays
        // Still vectorizable, but more memory streams
        float v_squared = particles.vx[i] * particles.vx[i] +
                         particles.vy[i] * particles.vy[i] +
                         particles.vz[i] * particles.vz[i];
        total += v_squared;
    }
    
    return total * 0.5f;
}

int main() {
    std::cout << "=== Array of Structures (AoS) vs. Structure of Arrays (SoA) ===\n\n";
    
    // Configuration
    const size_t N = 10000000;  // 10 million particles
    const int num_runs = 10;
    const float dt = 0.016f;   // ~60 FPS timestep
    
    std::cout << "Number of particles: " << N << "\n";
    std::cout << "sizeof(ParticleAoS): " << sizeof(ParticleAoS) << " bytes\n";
    std::cout << "AoS total memory: " << (N * sizeof(ParticleAoS)) / (1024.0 * 1024.0) << " MB\n";
    std::cout << "SoA total memory: " << (N * 6 * sizeof(float)) / (1024.0 * 1024.0) << " MB\n\n";
    
    // Create and initialize AoS
    std::cout << "Initializing AoS data...\n";
    std::vector<ParticleAoS> aos(N);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (size_t i = 0; i < N; ++i) {
        aos[i].x = dist(gen); aos[i].y = dist(gen); aos[i].z = dist(gen);
        aos[i].vx = dist(gen); aos[i].vy = dist(gen); aos[i].vz = dist(gen);
    }
    
    // Create and initialize SoA (copy from AoS)
    std::cout << "Initializing SoA data...\n";
    ParticlesSoA soa;
    soa.resize(N);
    
    for (size_t i = 0; i < N; ++i) {
        soa.x[i] = aos[i].x; soa.y[i] = aos[i].y; soa.z[i] = aos[i].z;
        soa.vx[i] = aos[i].vx; soa.vy[i] = aos[i].vy; soa.vz[i] = aos[i].vz;
    }
    
    std::cout << "Data initialized.\n\n";
    
    // Warm-up
    update_positions_aos(aos, dt);
    update_positions_soa(soa, dt);
    
    // Test 1: Full position update (x, y, z)
    std::cout << "=== Test 1: Update All Positions (x, y, z) ===\n\n";
    
    // AoS version
    std::cout << "--- AoS Layout ---\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            update_positions_aos(aos, dt);
            total_time += timer.elapsed_ms();
            sink_float = aos[N/2].x;
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
    }
    
    // SoA version
    std::cout << "--- SoA Layout ---\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            update_positions_soa(soa, dt);
            total_time += timer.elapsed_ms();
            sink_float = soa.x[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n\n";
    }
    
    // Test 2: Partial update (x only)
    std::cout << "=== Test 2: Update X Position Only ===\n";
    std::cout << "(Maximum advantage for SoA)\n\n";
    
    // AoS version
    std::cout << "--- AoS Layout ---\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            update_x_only_aos(aos, dt);
            total_time += timer.elapsed_ms();
            sink_float = aos[N/2].x;
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Cache efficiency: ~33% (loading 24 bytes, using 8)\n";
    }
    
    // SoA version
    std::cout << "--- SoA Layout ---\n";
    {
        double total_time = 0;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            update_x_only_soa(soa, dt);
            total_time += timer.elapsed_ms();
            sink_float = soa.x[N/2];
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Cache efficiency: ~100% (loading only x and vx)\n\n";
    }
    
    // Test 3: Compute kinetic energy (uses all velocity components)
    std::cout << "=== Test 3: Compute Kinetic Energy ===\n";
    std::cout << "(Uses vx, vy, vz - partial field access)\n\n";
    
    // AoS version
    std::cout << "--- AoS Layout ---\n";
    {
        double total_time = 0;
        float energy;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            energy = compute_kinetic_energy_aos(aos);
            total_time += timer.elapsed_ms();
            sink_float = energy;
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Energy: " << energy << "\n";
    }
    
    // SoA version
    std::cout << "--- SoA Layout ---\n";
    {
        double total_time = 0;
        float energy;
        for (int run = 0; run < num_runs; ++run) {
            Timer timer;
            energy = compute_kinetic_energy_soa(soa);
            total_time += timer.elapsed_ms();
            sink_float = energy;
        }
        std::cout << "Average time: " << std::fixed << std::setprecision(2) 
                  << total_time / num_runs << " ms\n";
        std::cout << "Energy: " << energy << "\n\n";
    }
    
    // Summary
    std::cout << "=== Summary ===\n\n";
    
    std::cout << "Memory Layout Comparison:\n\n";
    
    std::cout << "AoS (Array of Structures):\n";
    std::cout << "  [x0 y0 z0 vx0 vy0 vz0] [x1 y1 z1 vx1 vy1 vz1] ...\n";
    std::cout << "  + Good when accessing all fields of one object\n";
    std::cout << "  + Natural OOP style\n";
    std::cout << "  - Wastes cache when accessing partial fields\n";
    std::cout << "  - Harder to vectorize (non-contiguous same-type data)\n\n";
    
    std::cout << "SoA (Structure of Arrays):\n";
    std::cout << "  x:  [x0 x1 x2 ...]\n";
    std::cout << "  y:  [y0 y1 y2 ...]\n";
    std::cout << "  vx: [vx0 vx1 vx2 ...]\n";
    std::cout << "  + Perfect cache utilization for partial field access\n";
    std::cout << "  + Easy to vectorize (contiguous same-type data)\n";
    std::cout << "  - Scattered access when need all fields of one object\n";
    std::cout << "  - Less natural programming style\n\n";
    
    std::cout << "When to use each:\n";
    std::cout << "  AoS: Single-object operations, infrequent bulk operations\n";
    std::cout << "  SoA: Data-parallel operations, SIMD, partial field access\n";
    
    return 0;
}
