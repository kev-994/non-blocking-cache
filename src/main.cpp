#include "Interconnect.h"
#include "MemoryController.h"
#include "Cache.h"
#include "CoherenceMessage.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

// Instruction Trace Structure
struct Instruction 
{
    std::uint64_t cpu_id;
    TargetType type;
    std::uint64_t address;
    std::uint8_t write_data;
    std::string description;
};

int main()
{
    Interconnect noc;
    MemoryController memory(&noc);
    
    Cache cache0(0, &noc, 2);
    Cache cache1(1, &noc, 2);

    noc.attachMemory(&memory);
    noc.attachCache(&cache0);
    noc.attachCache(&cache1);

    std::cout << "--- System Initialized ---\n\n";

    // Simulating a CPU instruction stream
    std::vector<Instruction> instruction_trace = 
    {
        {0, TargetType::Read,  0x1000, 0x00, "Cache 0 reads 0x1000 (Cold Miss -> Fetches from Memory)"},
        {0, TargetType::Write, 0x1000, 0xAA, "Cache 0 writes 0x1000 (Write Hit -> Upgrades to Modified)"},
        {1, TargetType::Read,  0x1000, 0x00, "Cache 1 reads 0x1000 (C2C Transfer -> Cache 0 intercepts & sends data)"},
        {1, TargetType::Write, 0x1000, 0xBB, "Cache 1 writes 0x1000 (Coherence Miss -> Invalidates Cache 0)"},
        {0, TargetType::Read,  0x1004, 0x00, "Cache 0 reads 0x1004 (Cold Miss on same block -> Fetches from Memory)"},
        {0, TargetType::Read,  0x2000, 0x00, "Cache 0 reads 0x2000 (Cold Miss on new block -> Tests capacity)"}
    };

    std::uint8_t read_data_out{0};

    // The CPU Execution Loop
    for (std::size_t i = 0; i < instruction_trace.size(); ++i)
    {
        const auto& inst = instruction_trace[i];
        std::cout << "[Cycle " << i << "] " << inst.description << "\n";

        RequestStatus status;
        if (inst.cpu_id == 0)
        {
            status = cache0.processCPURequest(inst.type, inst.address, inst.write_data, read_data_out);
        }
        else
        {
            status = cache1.processCPURequest(inst.type, inst.address, inst.write_data, read_data_out);
        }

        std::cout << "   Status: ";
        switch (status)
        {
            case RequestStatus::Hit:      std::cout << "Hit\n\n"; break;
            case RequestStatus::Miss:     std::cout << "Miss (MSHR Allocated)\n\n"; break;
            case RequestStatus::Rejected: std::cout << "Rejected (MSHRs Full)\n\n"; break;
        }
    }

    std::cout << "--- Trace Execution Complete ---\n";
    return 0;
}