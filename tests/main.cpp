#include "Interconnect.h"
#include "MemoryController.h"
#include "Cache.h"
#include "CoherenceMessage.h"

#include <cassert>
#include <iostream>
#include <iomanip>
#include <cstdint>

// Helper to format CPU trace logs cleanly
void logTrace(std::uint64_t cycle, int cpu, const char* op, std::uint64_t addr, RequestStatus status, const char* note = "")
{
    std::cout << "[CYC " << std::setw(3) << std::setfill('0') << cycle << "] | "
              << "CPU " << cpu << " | "
              << "OP: " << std::setw(5) << std::setfill(' ') << op << " | "
              << "ADDR: 0x" << std::hex << std::uppercase << addr << std::dec << " | "
              << "STAT: " << (status == RequestStatus::Hit ? "HIT " : "MISS") 
              << " " << note << "\n";
}

int main()
{
    Interconnect noc;
    MemoryController memory(&noc);
    
    // Instantiate 3 caches, max capacity of 4 blocks each to easily test LRU
    Cache cache0(0, &noc, 3);
    Cache cache1(1, &noc, 3);
    Cache cache2(2, &noc, 3);

    noc.attachMemory(&memory);
    noc.attachCache(&cache0);
    noc.attachCache(&cache1);
    noc.attachCache(&cache2);

    std::uint64_t current_cycle{0};
    const std::uint64_t MAX_CYCLES{300};
    std::uint8_t read_data{0};
    RequestStatus status;

    std::cout << "=======================================================\n";
    std::cout << " INITIATING HARDWARE SIMULATION & VERIFICATION SUITE   \n";
    std::cout << "=======================================================\n\n";

    while (current_cycle <= MAX_CYCLES)
    {
        current_cycle++;

        // 1. Rising Edge: Tick all hardware components
        noc.tick();
        memory.tick();
        cache0.tick();
        cache1.tick();
        cache2.tick();

        // ---------------------------------------------------------
        // PHASE 1: Cold Read & MSHR Target Merging (Cycles 1 - 15)
        // ---------------------------------------------------------
        if (current_cycle == 1)
        {
            status = cache0.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
            logTrace(current_cycle, 0, "READ", 0x1000, status, "(Cold Miss, MSHR Allocated)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 2)
        {
            status = cache0.processCPURequest(TargetType::Read, 0x1004, 0, read_data);
            logTrace(current_cycle, 0, "READ", 0x1004, status, "(Secondary Miss, Merged into MSHR)");
            assert(status == RequestStatus::Miss); 
        }
        if (current_cycle == 3)
        {
            status = cache0.processCPURequest(TargetType::Write, 0x1008, 0xAA, read_data);
            logTrace(current_cycle, 0, "WRITE", 0x1008, status, "(Tertiary Miss, Merged. Transient state escalated)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 15)
        {
            std::cout << "  -- Mem Latency Resolved (Block 0x1000) --\n";
            status = cache0.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
            logTrace(current_cycle, 0, "READ", 0x1008, status, "(Validating MSHR Write Target)");
            assert(status == RequestStatus::Hit);
            assert(read_data == 0xAA);
        }

        // ---------------------------------------------------------
        // PHASE 2: C2C Transfer & Coherence Downgrade (Cycles 20 - 25)
        // ---------------------------------------------------------
        if (current_cycle == 20)
        {
            status = cache1.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x1000, status, "(C2C Fetch. Cache 0 supplies data & downgrades to Shared)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 22)
        {
            status = cache1.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x1008, status, "(Validating C2C transfer integrity)");
            assert(status == RequestStatus::Hit);
            assert(read_data == 0xAA); 
        }

        // ---------------------------------------------------------
        // PHASE 3: Coherence Miss (Silent Upgrade) (Cycles 30 - 35)
        // ---------------------------------------------------------
        if (current_cycle == 30)
        {
            status = cache1.processCPURequest(TargetType::Write, 0x1008, 0xBB, read_data);
            logTrace(current_cycle, 1, "WRITE", 0x1008, status, "(Coherence Miss. Broadcasts SnoopInvalidate)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 32)
        {
            // SAFE VALIDATION: Using backdoor checkState to avoid triggering the observer effect.
            assert(cache0.checkState(0x1008) == MESIState::Invalid);
            std::cout << "  [PASS] Passive Verification: Cache 0 successfully invalidated 0x1000.\n";
        }

        // ---------------------------------------------------------
        // PHASE 4: Cold Write (RFO) & RFO Interception (Cycles 40 - 65)
        // ---------------------------------------------------------
        if (current_cycle == 40)
        {
            status = cache2.processCPURequest(TargetType::Write, 0x2000, 0xCC, read_data);
            logTrace(current_cycle, 2, "WRITE", 0x2000, status, "(Cold Write RFO)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 55)
        {
            std::cout << "  -- Mem Latency Resolved (Block 0x2000) --\n";
            status = cache2.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
            logTrace(current_cycle, 2, "READ", 0x2000, status, "(Validating RFO target applied)");
            assert(status == RequestStatus::Hit);
            assert(read_data == 0xCC);
        }
        if (current_cycle == 60)
        {
            status = cache1.processCPURequest(TargetType::Write, 0x2004, 0xDD, read_data);
            logTrace(current_cycle, 1, "WRITE", 0x2004, status, "(RFO Intercept. Stealing Modified block from Cache 2)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 65)
        {
            // SAFE VALIDATION
            assert(cache2.checkState(0x2000) == MESIState::Invalid);
            std::cout << "  [PASS] Passive Verification: Cache 2 successfully invalidated 0x2000.\n";

            status = cache1.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x2000, status, "(Validating stolen dirty payload)");
            assert(status == RequestStatus::Hit);
            assert(read_data == 0xCC); // Ensuring Cache 1 successfully kept Cache 2's dirty data
        }

        // ---------------------------------------------------------
        // PHASE 5: Saturation, LRU Eviction & Dirty Writeback (Cycles 70 - 150)
        // ---------------------------------------------------------
        if (current_cycle == 70)
        {
            std::cout << "\n  -- Saturating Cache 1 to force LRU Eviction --\n";
            status = cache1.processCPURequest(TargetType::Read, 0x3000, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x3000, status, "(Filling slot 3)");
        }
        if (current_cycle == 90)
        {
            status = cache1.processCPURequest(TargetType::Read, 0x4000, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x4000, status, "(Filling slot 4. Cache 1 is now FULL)");
        }
        if (current_cycle == 110)
        {
            // Block 0x1000 is the oldest (last touched at Cycle 30). It is in Modified state (val: 0xBB).
            status = cache1.processCPURequest(TargetType::Read, 0x5000, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x5000, status, "(Cache Overflow! Evicting LRU Block 0x1000 & Writing Back)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 125)
        {
            // SAFE VALIDATION
            assert(cache1.checkState(0x1000) == MESIState::Invalid);
            std::cout << "  [PASS] Passive Verification: 0x1000 was physically evicted from Cache 1.\n";
        }
        if (current_cycle == 130)
        {
            // Fetching 0x1000 from memory to prove the Writeback successfully preserved the 0xBB write.
            status = cache0.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
            logTrace(current_cycle, 0, "READ", 0x1008, status, "(Fetching evicted block from Memory)");
            assert(status == RequestStatus::Miss); // Should correctly miss because memory is delayed
        }
        if (current_cycle == 145)
        {
            status = cache0.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
            logTrace(current_cycle, 0, "READ", 0x1008, status, "(Validating memory captured the dirty writeback)");
            assert(status == RequestStatus::Hit);
            assert(read_data == 0xBB);
        }

        // ---------------------------------------------------------
        // PHASE 6: Thrashing, Concurrency & Extreme MSHR Stress (Cycles 160 - 300)
        // ---------------------------------------------------------
        if (current_cycle == 160)
        {
            std::cout << "\n  -- Initiating Thrashing & Concurrency Stress Test --\n";
            status = cache2.processCPURequest(TargetType::Read, 0x6000, 0, read_data);
            logTrace(current_cycle, 2, "READ", 0x6000, status, "(Thrashing Phase Start)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 162)
        {
            // Fire overlapping requests across multiple caches while memory is busy
            status = cache1.processCPURequest(TargetType::Read, 0x7000, 0, read_data);
            logTrace(current_cycle, 1, "READ", 0x7000, status, "(Concurrent Fetch)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 180)
        {
            status = cache2.processCPURequest(TargetType::Read, 0x8000, 0, read_data);
            logTrace(current_cycle, 2, "READ", 0x8000, status, "(Filling Cache 2)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 200)
        {
            status = cache2.processCPURequest(TargetType::Read, 0x9000, 0, read_data);
            logTrace(current_cycle, 2, "READ", 0x9000, status, "(Filling Cache 2)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 220)
        {
            status = cache2.processCPURequest(TargetType::Write, 0xA000, 0xFF, read_data);
            logTrace(current_cycle, 2, "WRITE", 0xA000, status, "(Forcing LRU Eviction under stress)");
            assert(status == RequestStatus::Miss);
        }
        if (current_cycle == 235)
        {
            std::cout << "  -- Mem Latency Resolved (Block 0xA000) --\n";
            status = cache2.processCPURequest(TargetType::Read, 0xA000, 0, read_data);
            logTrace(current_cycle, 2, "READ", 0xA000, status, "(Validating Write target survived thrashing)");
            assert(status == RequestStatus::Hit);
            assert(read_data == 0xFF);
        }
    }

    std::cout << "\n=======================================================\n";
    std::cout << " ALL 300 CYCLES EXECUTED. ASSERTIONS PASSED SUCCESSFULLY! \n";
    std::cout << "=======================================================\n";

    return 0;
}