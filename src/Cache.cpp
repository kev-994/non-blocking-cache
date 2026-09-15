#include "Cache.h"
#include "Interconnect.h"

RequestStatus Cache::processCPURequest(TargetType type, std::uint64_t address, std::uint8_t write_data, std::uint8_t& read_data_out)
{
    const std::uint64_t tag{address >> 6};
    const std::uint8_t offset{static_cast<std::uint8_t>(address & 0x3F)}; // gives the lower 6 bits (64 byte line)
    const std::uint64_t block_address{address & ~0x3F};

    // ^ hardcoded

    bool is_upgrade{};
    
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
                        block.state = MESIState::Modified;
                        return RequestStatus::Hit;
                    }
                    case MESIState::Shared: // coherence miss, this is a read only copy
                    {
                        is_upgrade = true;
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
    std::size_t expected_acks{type == TargetType::Write ? m_total_caches - 1 : 0};
    MSHREntry mshr_entry{block_address, transient_state, true, expected_acks};
    mshr_entry.data_received = is_upgrade; // the data is sitting in cache already (coherence miss)
    mshr_entry.targets.push_back(target);

    m_MSHRFile.push_back(mshr_entry);

    // construct coherence message
    MessageType msg_type{type == TargetType::Read ? MessageType::ReadRequest : is_upgrade ? MessageType::WriteRequest : MessageType::ReadyForOwnership};

    CoherenceMessage msg{msg_type, block_address, block_address, m_cache_id, m_cache_id, write_data}; // block_address used as transaction_id as mshrs guarantee only one active fetch per block
    m_interconnect->routeMessage(msg);
    
    return RequestStatus::Miss;
}

bool Cache::processBusMessage(const CoherenceMessage& msg)
{
    const auto tag{msg.address >> 6};
    bool intercepted{};
    
    switch (msg.type)
    {
        case MessageType::ReadResponse:
        {
            bool block_updated{};
            
            for (std::size_t i{}; i < m_MSHRFile.size(); ++i)
            {
                if (m_MSHRFile[i].block_address == msg.transaction_id)
                {
                    m_MSHRFile[i].data_received = true;
                    m_MSHRFile[i].fetch_buffer = msg.data;

                    if (m_MSHRFile[i].acks_remaining == 0)
                    {
                        bool block_updated{};

                        for (auto& block : m_cache_blocks)
                        {
                            if (block.tag == tag) // coherence upgrade
                            {
                                block.state = MESIState::Modified;
                                for (auto& target : m_MSHRFile[i].targets)
                                {
                                    if (target.type == TargetType::Write)
                                    {
                                        block.data.data[target.offset] = target.write_data;
                                    }
                                }
                                block_updated = true;
                                break;
                            }
                        }

                        if (!block_updated) // cold read/write miss
                        {
                            MESIState state = (m_MSHRFile[i].transient_state == MESIState::InvalidShared) 
                                            ? (msg.is_shared ? MESIState::Shared : MESIState::Exclusive) 
                                            : MESIState::Modified;
                                            
                            // pull from fetch_buffer, not msg.data, in case this was triggered by a SnoopAck
                            CacheBlock new_block{tag, state, m_MSHRFile[i].fetch_buffer};
                            
                            for (auto& target : m_MSHRFile[i].targets)
                            {
                                if (target.type == TargetType::Write)
                                {
                                    new_block.data.data[target.offset] = target.write_data;
                                }
                            }
                            m_cache_blocks.push_back(new_block);
                        }

                        m_MSHRFile.erase(m_MSHRFile.begin() + i);
                        break; 
                    }
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
                    // ignore the snoop if we don't actually hold a valid copy of the block
                    if (block.state != MESIState::Invalid)
                    {
                        if (block.state == MESIState::Modified)
                        {
                            CoherenceMessage coherence_message{MessageType::Writeback, msg.address, msg.address, m_cache_id, msg.sender_id, block.data};
                            intercepted = true;
                            m_interconnect->routeMessage(coherence_message);
                        }
                        
                        block.state = MESIState::Shared;
                    }
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
                    block.state = MESIState::Invalid;

                    break;
                }
            }

            CoherenceMessage coherence_message{MessageType::SnoopAck, msg.address, msg.address, m_cache_id, msg.sender_id}; // doesn't need to send data
            m_interconnect->routeMessage(coherence_message);

            break;
        }

        case MessageType::SnoopAck:
        {
            bool block_updated{};
            
            for (std::size_t i{}; i < m_MSHRFile.size(); ++i)
            {
                if (m_MSHRFile[i].block_address == msg.transaction_id)
                {
                    m_MSHRFile[i].acks_remaining--;

                    if (m_MSHRFile[i].data_received == true && m_MSHRFile[i].acks_remaining == 0)
                    {
                        bool block_updated{};

                        for (auto& block : m_cache_blocks)
                        {
                            if (block.tag == tag) // coherence upgrade
                            {
                                block.state = MESIState::Modified;
                                for (auto& target : m_MSHRFile[i].targets)
                                {
                                    if (target.type == TargetType::Write)
                                    {
                                        block.data.data[target.offset] = target.write_data;
                                    }
                                }
                                block_updated = true;
                                break;
                            }
                        }

                        if (!block_updated) // cold read/write miss
                        {
                            MESIState state = (m_MSHRFile[i].transient_state == MESIState::InvalidShared) 
                                            ? (msg.is_shared ? MESIState::Shared : MESIState::Exclusive) 
                                            : MESIState::Modified;
                                            
                            // pull from fetch_buffer, not msg.data, in case this was triggered by a SnoopAck
                            CacheBlock new_block{tag, state, m_MSHRFile[i].fetch_buffer};
                            
                            for (auto& target : m_MSHRFile[i].targets)
                            {
                                if (target.type == TargetType::Write)
                                {
                                    new_block.data.data[target.offset] = target.write_data;
                                }
                            }
                            m_cache_blocks.push_back(new_block);
                        }

                        m_MSHRFile.erase(m_MSHRFile.begin() + i);
                        break; 
                    }
                }
            }
            break;
        }

        case MessageType::SnoopRFO:
        {
            for (auto& block : m_cache_blocks)
            {
                if (block.tag == tag)
                {
                    if (block.state == MESIState::Modified)
                    {
                        CoherenceMessage writeback{MessageType::Writeback, msg.address, msg.address, m_cache_id, msg.sender_id, block.data};
                        m_interconnect->routeMessage(writeback);

                        CoherenceMessage ack{MessageType::SnoopAck, msg.address, msg.address, m_cache_id, msg.sender_id};
                        m_interconnect->routeMessage(ack);

                        block.state = MESIState::Invalid;
                        intercepted = true;
                        break;
                    }

                    block.state = MESIState::Invalid;
                    break;
                }
            }
            CoherenceMessage ack{MessageType::SnoopAck, msg.address, msg.address, m_cache_id, msg.sender_id}; // doesn't need to send data
            m_interconnect->routeMessage(ack);

            break;
        }
    }

    return intercepted;
}

/*
when a snoop hits an active MSHR, it means two different caches have suffered a miss on the exact same block
at the exact same time, creating a race condition.

safely bypass this by designing the Interconnect as a smart directory that serializes requests
and prevents these collisions from reaching the caches in the first place
*/

bool Cache::hasValidBlock(std::uint64_t address) const
{
    const auto search_tag{address >> 6};
    for (const auto& block : m_cache_blocks)
    {
        if (block.tag == search_tag && block.state != MESIState::Invalid) 
        {
            return true;
        }
    }
    return false;
}