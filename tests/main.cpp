#include "Interconnect.h"
#include "MemoryController.h"
#include "Cache.h"
#include "CoherenceMessage.h"

#include <cassert>
#include <iostream>

int main()
{
    Interconnect noc;
    MemoryController memory(&noc);
    
    Cache cache0(0, &noc, 3);
    Cache cache1(1, &noc, 3);
    Cache cache2(2, &noc, 3);

    noc.attachMemory(&memory);
    noc.attachCache(&cache0);
    noc.attachCache(&cache1);
    noc.attachCache(&cache2);

    std::uint8_t read_data{0};
    RequestStatus status;

    // --- TEST 1: Cold Read & Exclusive State ---
    status = cache0.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Miss);
    status = cache0.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0x00);

    // --- TEST 2: Silent Upgrade (Exclusive -> Modified) ---
    status = cache0.processCPURequest(TargetType::Write, 0x1004, 0xAA, read_data);
    assert(status == RequestStatus::Hit); 

    // --- TEST 3: C2C Transfer & Downgrade (Modified -> Shared) ---
    status = cache1.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Miss); 
    status = cache1.processCPURequest(TargetType::Read, 0x1004, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xAA); 

    // --- TEST 4: Coherence Upgrade (Shared -> Modified) ---
    status = cache0.processCPURequest(TargetType::Write, 0x1008, 0xBB, read_data);
    assert(status == RequestStatus::Miss); 
    status = cache1.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
    assert(status == RequestStatus::Miss); // Cache 1 was invalidated

    // --- TEST 5: Cold Write Miss (Read-For-Ownership) ---
    status = cache2.processCPURequest(TargetType::Write, 0x2000, 0xCC, read_data);
    assert(status == RequestStatus::Miss); 
    status = cache2.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xCC);

    // --- TEST 6: RFO Interception (Dirty Block Stolen) ---
    status = cache0.processCPURequest(TargetType::Write, 0x2004, 0xDD, read_data);
    assert(status == RequestStatus::Miss); 
    status = cache2.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Miss); // Cache 2 was invalidated
    status = cache0.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xCC); 

    // --- TEST 7: LRU Eviction & Dirty Writeback ---
    // Cache 0 currently holds 2 blocks: 0x1000 (M) and 0x2000 (M). 
    // Let's fetch 3 completely new blocks to force an overflow (assuming max_blocks = 4).
    status = cache0.processCPURequest(TargetType::Read, 0x3000, 0, read_data);
    assert(status == RequestStatus::Miss);
    status = cache0.processCPURequest(TargetType::Read, 0x4000, 0, read_data);
    assert(status == RequestStatus::Miss);
    
    // The cache is now full (4 blocks). This 5th fetch will evict the LRU block (0x1000).
    status = cache0.processCPURequest(TargetType::Read, 0x5000, 0, read_data);
    assert(status == RequestStatus::Miss);

    // Verification 1: Cache 0 no longer has 0x1000.
    status = cache0.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Miss);

    // Verification 2: Cache 0 wrote 0x1000 back to memory during eviction.
    // When Cache 1 fetches it, it should receive the dirty data (0xBB) from memory.
    status = cache1.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Miss); // Cache 1 fetches from Memory
    status = cache1.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xBB); // The dirty data survived the eviction!

    std::cout << "All exhaustive architectural assertions passed successfully.\n";
    return 0;
}