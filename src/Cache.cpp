#include "Cache.h"

RequestStatus Cache::processCPURequest(TargetType type, std::uint64_t address, std::uint8_t write_data, std::uint8_t& read_data_out)
{
    const std::uint64_t tag{address >> 6};
    const std::uint64_t offset{address & 0x3F}; // gives the lower 6 bits (64 byte line)
    const std::uint64_t block_address{address & ~0x3F};

    // ^ hardcoded
    
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
    const auto tag{msg.address >> 6};
    
    switch (msg.type)
    {
        case MessageType::ReadResponse:
        {
            for (std::size_t i{}; i < m_MSHRFile.size(); ++i)
            {
                if (m_MSHRFile[i].block_address == msg.transaction_id)
                {
                    MESIState state{m_MSHRFile[i].transient_state == MESIState::InvalidShared ? MESIState::Shared : MESIState::Modified};
                    
                    CacheBlock block{tag, state, msg.data};

                    for (auto& target : m_MSHRFile[i].targets)
                    {
                        if (target.type == TargetType::Write)
                        {
                            block.data.data[target.offset] = target.write_data;
                            block.state = MESIState::Modified;
                        }

                        else if (target.type == TargetType::Read)
                        {
                            forwardToCPU(target.requester_id, block.data.data[target.offset]);
                        }
                    }

                    m_cache_blocks.push_back(block);
                    m_MSHRFile.erase(m_MSHRFile.begin() + i); // remove resolved entry
                    break;
                }
            }
            break;
        }
        
        case MessageType::SnoopRead:
        {
            for (auto& block : m_cache_blocks)
            {
                if (block.tag == tag)
                {
                    if (block.state == MESIState::Modified)
                    {
                        CoherenceMessage coherence_message{MessageType::Writeback, msg.address, msg.address, m_cache_id, msg.sender_id, msg.data};
                        m_interconnect->routeMessage(coherence_message);
                    }

                    block.state = MESIState::Shared;
                    break;
                }
            }
            break;
        }

        case MessageType::SnoopInvalidate:
        {
            for (auto& block : m_cache_blocks)
            {
                if (block.tag == tag)
                {
                    if (block.state != MESIState::Invalid)
                    {
                        block.state = MESIState::Invalid;
                    }

                    CoherenceMessage coherence_message{MessageType::SnoopAck, msg.address, msg.address, m_cache_id, msg.sender_id}; // doesn't need to send data
                    m_interconnect->routeMessage(coherence_message);

                    break;
                }
            }
            break;
        }
    }
}