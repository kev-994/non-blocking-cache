# Multi-Cache Coherence Simulator

A cycle-driven, event-based simulator of a multiple caches implementing the **MESI coherence protocol** with **non-blocking cache** behaviour via **Miss Status Holding Registers (MSHRs)**. The model captures the request/response and snoop traffic between private L1-style caches, a shared interconnect, and a latency-modelled backing memory.

## Overview

Each core owns a private, fully-associative cache. Caches communicate coherence state exclusively through an `Interconnect`, which broadcasts snoops to sibling caches and forwards requests to a `MemoryController` when no cache can service them. Misses do not stall the cache: outstanding requests are tracked in MSHRs, secondary misses to an in-flight block are merged into the existing entry's target queue, and the cache remains free to accept requests for other blocks (up to a configurable MSHR limit).

## Key Features

- **MESI protocol** — Modified, Exclusive, Shared, Invalid, plus transient `InvalidShared` / `InvalidModified` states for in-flight misses.
- **Non-blocking misses (MSHRs)** — up to 4 outstanding misses per cache; secondary/tertiary requests to the same block merge into one MSHR entry rather than reissuing bus traffic.
- **Snoop-based coherence** — broadcast `SnoopRead`, `SnoopInvalidate`, and `SnoopRFO` (Read-for-Ownership) with `SnoopAck` completion tracking.
- **Cache-to-cache transfers** — a cache holding a Modified line intercepts a sibling's request and supplies data directly via writeback, bypassing memory.
- **LRU eviction with writeback** — capacity misses evict the least-recently-used block, flushing dirty (Modified) data to memory first.
- **Latency-modelled memory** — `MemoryController` queues requests and resolves them after a fixed simulated latency, exercising the non-blocking path under realistic delay.

## Architecture

```
CPU requests
     │
     ▼
  ┌───────┐   coherence msgs    ┌──────────────┐   miss / writeback   ┌──────────────────┐
  │ Cache │ ───────────────────▶│ Interconnect │ ─────────────────────▶│ MemoryController  │
  │ (×N)  │◀─────────────────── │  (snoop bus)  │◀───────────────────── │ (latency-modelled)│
  └───────┘   snoops / acks     └──────────────┘     read responses    └──────────────────┘
```

- **`Cache`** owns all MESI state transitions and its MSHR file. It decides hit/miss/reject locally and only emits a `CoherenceMessage` on a miss.
- **`Interconnect`** is a stateless router: it fans out snoops to sibling caches, collects intercept/shared-wire signals, and decides whether a request needs to go to memory.
- **`MemoryController`** models DRAM as a sparse backing store (`unordered_map`, since most of the address space is never touched) with a fixed-cycle response latency and an in-order pending-request queue.

## Project Structure

```
include/
  CacheLine.h          # fixed-size cache line storage (64 bytes)
  CoherenceMessage.h    # MESI states, message types, bus message struct
  MSHR.h                # miss status holding register + pending target entries
  Cache.h                # per-core cache: MESI logic, MSHR file, LRU
  Interconnect.h        # snoop bus / message router
  MemoryController.h    # latency-modelled backing store

src/
  Cache.cpp
  Interconnect.cpp
  MemoryController.cpp

tests/
  main.cpp              # cycle-driven functional testbench

CMakeLists.txt
```

## Building

Requires a C++20 compiler and CMake ≥ 3.20.

```bash
cmake -S . -B build
cmake --build build
./build/main
```

## Testing

`tests/main.cpp` is a self-checking, cycle-scheduled testbench (300 simulated cycles) that drives all three caches through a sequence of scenarios and asserts on architectural state at each checkpoint:

1. **Cold read + MSHR target merging** — multiple pending requests to the same block collapse into one MSHR entry.
2. **Cache-to-cache transfer** — a sibling cache supplies dirty data directly, without a memory round-trip.
3. **Coherence upgrade miss** — a Shared→Modified write triggers `SnoopInvalidate` across siblings.
4. **Read-for-Ownership** — a cold write allocates via RFO; a later RFO from another cache steals the Modified line mid-flight.
5. **LRU eviction and writeback** — a full cache evicts its oldest block, flushing dirty data to memory, and a subsequent fetch confirms the writeback landed.
6. **Concurrency/thrashing stress** — overlapping outstanding misses across caches while memory is still resolving earlier requests, exercising MSHR capacity and ordering.

Each checkpoint logs a formatted trace line (cycle, initiating core, operation, address, hit/miss) and asserts on either the returned `RequestStatus` or on cache state read back via `checkState()`.

## Design Notes

- Address decomposition assumes a fixed 64-byte line size (6-bit offset); tag/offset split is currently hardcoded rather than parameterised.
- The interconnect intentionally centralises snoop ordering to avoid the race condition where two caches miss on the same block in the same cycle — this is called out directly in the `Cache.cpp` source comments.
- MSHR target queues are unbounded `std::vector`s for simplicity; a real implementation would bound this by CPU issue width / ROB depth.
