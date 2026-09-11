#include "Cache.h"

RequestStatus Cache::processCPURequest(TargetType type, std::uint64_t address, std::uint8_t write_data, std::uint8_t& read_data_out)
{
    std::uint64_t tag{address >> 6};
    std::uint64_t offset{address & 0x3F}; // gives the lower 6 bits (64 byte line)
    std::uint64_t block_address{address & ~0x3F};
    
    for (auto& block : m_cache_blocks)
    {
        // hit
        if (block.tag == tag && block.state != MESIState::Invalid) 
        {
            // read hit
            if (type == TargetType::Read)
            {
                read_data_out = block.data.data[offset];
                return RequestStatus::Hit;
            }

            // write hit
            else if (type == TargetType::Write)
            {
                switch (block.state)
                {
                    case MESIState::Exclusive: [[fallthrough]];
                    case MESIState::Modified:
                    {
                        block.data.data[offset] = write_data;
                        return RequestStatus::Hit;
                    }
                    case MESIState::Shared: // coherence miss, this is a read only copy
                    {
                        block.state = MESIState::InvalidModified;
                        break;
                    }
                }
            }
        }
    }

    Target target{type, m_cache_id, offset, write_data};

    // secondary miss, mshr look up
    
    for (auto& mshr : m_MSHRFile)
    {
        if(mshr.block_address == block_address)
        {
            mshr.targets.push_back(target);
            return RequestStatus::Miss;
        }
    }

    // primary miss
    if (m_MSHRFile.size() >= max_mshrs)
    {
        return RequestStatus::Rejected; // can't accept any more misses
    }

    MESIState transient_state{type == TargetType::Read ? MESIState::InvalidShared : MESIState::InvalidModified};

    MSHREntry mshr_entry{block_address, transient_state, true};
    mshr_entry.targets.push_back(target);

    m_MSHRFile.push_back(mshr_entry);

    // construct coherence message
    MessageType msg_type{type == TargetType::Read ? MessageType::ReadRequest : MessageType::WriteRequest};

    CoherenceMessage msg{msg_type, block_address, block_address, m_cache_id, m_cache_id, write_data}; // block_address used as transaction_id as mshrs guarantee only one active fetch per block
    m_interconnect->routeMessage(msg);
    
    return RequestStatus::Miss;
}

void Cache::processBusMessage(const CoherenceMessage& msg)
{
    
}