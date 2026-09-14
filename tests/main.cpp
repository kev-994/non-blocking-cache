#include "Interconnect.h"
#include "MemoryController.h"
#include "Cache.h"
#include "CoherenceMessage.h"

#include <cassert>
#include <iostream>

int main()
{
    // 1. Component Instantiation (3-Cache Topology)
    Interconnect noc;
    MemoryController memory(&noc);
    
    // Initializing with total_caches = 3 to test the acks_remaining counter
    Cache cache0(0, &noc, 3);
    Cache cache1(1, &noc, 3);
    Cache cache2(2, &noc, 3);

    noc.attachMemory(&memory);
    noc.attachCache(&cache0);
    noc.attachCache(&cache1);
    noc.attachCache(&cache2);

    std::uint8_t read_data{0};
    RequestStatus status;

    // --- TEST 1: Byte-Level Offsets & Write Hits ---
    // Cache 0 reads block 0x2000 (Cold Miss)
    status = cache0.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Miss);
    
    // Cache 0 writes to offset 4 of block 0x2000 (Coherence Miss: Shared -> Modified)
    status = cache0.processCPURequest(TargetType::Write, 0x2004, 0xDD, read_data);
    assert(status == RequestStatus::Miss); 

    // Cache 0 writes to offset 8 of block 0x2000 (Write Hit: Already Modified)
    status = cache0.processCPURequest(TargetType::Write, 0x2008, 0xEE, read_data);
    assert(status == RequestStatus::Hit); 

    // --- TEST 2: C2C Transfer of Specific Offsets ---
    // Cache 1 reads offset 4. Cache 0 must intercept and supply the block.
    status = cache1.processCPURequest(TargetType::Read, 0x2004, 0, read_data);
    assert(status == RequestStatus::Miss);
    
    // Verify Cache 1 received the exact byte written by Cache 0 at offset 4
    status = cache1.processCPURequest(TargetType::Read, 0x2004, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xDD); 
    
    // Verify Cache 1 also received the byte at offset 8 within the same 64-byte payload
    status = cache1.processCPURequest(TargetType::Read, 0x2008, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xEE); 

    // --- TEST 3: Multi-Cache Invalidation (SnoopAck counting) ---
    // Cache 0 and 1 now hold 0x2000 in Shared. Cache 2 reads it (C2C or Memory fetch).
    status = cache2.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Miss);
    
    // Cache 2 writes to 0x2000. It must broadcast SnoopInvalidate and receive exactly 2 Acks.
    status = cache2.processCPURequest(TargetType::Write, 0x2000, 0xFF, read_data);
    assert(status == RequestStatus::Miss); 

    // Verify Cache 0 and 1 were successfully forced into the Invalid state
    status = cache0.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Miss); 
    status = cache1.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Miss); 

    // --- TEST 4: Cold Write Miss (The RFO limitation) ---
    // Cache 0 writes to a completely new block 0x3000. 
    status = cache0.processCPURequest(TargetType::Write, 0x3000, 0x11, read_data);
    assert(status == RequestStatus::Miss);
    
    // Verify the write applied to the locally created block
    status = cache0.processCPURequest(TargetType::Read, 0x3000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0x11);

    std::cout << "All deep architectural assertions passed successfully.\n";
    return 0;
}