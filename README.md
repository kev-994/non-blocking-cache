# Non-Blocking Cache Hierarchy with MSHRs & MESI

A cycle-accurate, multi-core cache hierarchy simulator written in C++. This project models the complex hardware state machines, asynchronous memory latencies, and concurrent miss resolutions characteristic of high-performance CPU architectures and low-latency systems. 

The simulator acts as a rigorously verified software blueprint for a hardware design, proving out the logic flows and race conditions required to sustain concurrent memory accesses without deadlocking.

## Key Architectural Features

*   **Non-Blocking Execution (MSHRs):** Implements Miss Status Handling Registers (MSHRs) to track outstanding memory requests. It gracefully absorbs and merges secondary or tertiary overlapping CPU targets (reads/writes) to the same block while the primary fetch is pending, preventing pipeline stalls.
*   **MESI Coherence Protocol:** Fully implements a distributed MESI (Modified, Exclusive, Shared, Invalid) state machine. It handles Cache-to-Cache (C2C) transfers, Read-For-Ownership (RFO) requests, silent upgrades, and broadcast invalidations over a simulated snooping bus.
*   **Cycle-Accurate Memory Latency:** Replaces instantaneous software returns with a global hardware clock. The memory controller utilizes a delayed hardware queue to enforce strict physical access latencies (e.g., 10-cycle delay), forcing the caches to asynchronously manage in-flight transactions.
*   **Timestamp-Based LRU Eviction:** Enforces strict physical cache capacity constraints. Utilizes a simulated access counter to identify and evict the Least Recently Used (LRU) block upon saturation, automatically routing Writeback messages for dirty (`Modified`) payloads.

## Project Structure

The codebase is modular, cleanly separating the state definitions, the hardware components, and the testbench driver.

*   **`include/`**
    *   `CoherenceMessage.h` & `CacheLine.h`: Core data structures representing physical wires and payloads.
    *   `MSHR.h`: Definitions for the Miss Status Handling Registers and pending CPU targets.
    *   `Cache.h`: The cache controller modeling the MESI state machine, tag lookup, and MSHR allocation.
    *   `Interconnect.h`: The snooping bus modeling broadcast routing and intercept logic.
    *   `MemoryController.h`: The asynchronous DRAM model using a sparse hash-map and time-delayed response queue.
*   **`src/`**
    *   Contains the `.cpp` implementations for the `Cache`, `Interconnect`, and `MemoryController`. 
*   **`tests/`**
    *   `main.cpp`: A comprehensive 300-cycle stimulus testbench. It injects specific overlapping reads and writes, prints a CPU-style trace to the terminal, and executes passive backdoor assertions (`checkState`) to verify data integrity.

## Getting Started

### Prerequisites
*   CMake (3.20 or higher)
*   A C++ compiler that supports C++20 (e.g., GCC, Clang, or MSVC).

### Build & Run
This project uses CMake for its build system. Run the following commands from the root of the repository:

```bash
# Generate the build system
cmake -S . -B build

# Compile the project
cmake --build build

# Execute the simulation
./build/main