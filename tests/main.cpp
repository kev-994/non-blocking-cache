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
    // Cache 0 fetches empty address 0x1000. It is not shared, so it becomes Exclusive.
    status = cache0.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Miss);
    
    status = cache0.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0x00);

    // --- TEST 2: Silent Upgrade (Exclusive -> Modified) ---
    // Cache 0 writes to its Exclusive block. No bus traffic generated.
    status = cache0.processCPURequest(TargetType::Write, 0x1004, 0xAA, read_data);
    assert(status == RequestStatus::Hit); 

    // --- TEST 3: C2C Transfer & Downgrade (Modified -> Shared) ---
    // Cache 1 reads 0x1000. Cache 0 must intercept SnoopRead, supply data, and downgrade to Shared.
    status = cache1.processCPURequest(TargetType::Read, 0x1000, 0, read_data);
    assert(status == RequestStatus::Miss); 
    
    status = cache1.processCPURequest(TargetType::Read, 0x1004, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xAA); 

    // --- TEST 4: Coherence Upgrade (Shared -> Modified) ---
    // Cache 0 writes to 0x1000. It is Shared, so it sends WriteRequest (SnoopInvalidate).
    status = cache0.processCPURequest(TargetType::Write, 0x1008, 0xBB, read_data);
    assert(status == RequestStatus::Miss); 
    
    // Verification: Cache 1 was invalidated and must miss on its next read.
    status = cache1.processCPURequest(TargetType::Read, 0x1008, 0, read_data);
    assert(status == RequestStatus::Miss); 

    // --- TEST 5: Cold Write Miss (Read-For-Ownership) ---
    // Cache 2 writes to a new address 0x2000. Broadcasts SnoopRFO and fetches from memory.
    status = cache2.processCPURequest(TargetType::Write, 0x2000, 0xCC, read_data);
    assert(status == RequestStatus::Miss); 
    
    status = cache2.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xCC);

    // --- TEST 6: RFO Interception (Dirty Block Stolen) ---
    // Cache 0 writes to 0x2000. Cache 2 holds it in Modified. 
    // Cache 2 must intercept SnoopRFO, fire a Writeback, send an Ack, and invalidate itself.
    status = cache0.processCPURequest(TargetType::Write, 0x2004, 0xDD, read_data);
    assert(status == RequestStatus::Miss); 
    
    // Verification: Cache 2 must now be Invalid.
    status = cache2.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Miss);

    // Verification: Cache 0's block must contain Cache 2's dirty data PLUS its own target write.
    status = cache0.processCPURequest(TargetType::Read, 0x2000, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xCC); // Cache 2's old write

    status = cache0.processCPURequest(TargetType::Read, 0x2004, 0, read_data);
    assert(status == RequestStatus::Hit);
    assert(read_data == 0xDD); // Cache 0's new write

    std::cout << "All exhaustive architectural assertions passed successfully.\n";
    return 0;
}