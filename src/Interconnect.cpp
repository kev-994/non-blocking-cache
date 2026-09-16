#include "Interconnect.h"
#include "Cache.h"
#include "MemoryController.h"

void Interconnect::attachCache(Cache* cache)
{
    m_caches.push_back(cache);
}

void Interconnect::attachMemory(MemoryController* memory)
{
    m_memory = memory;
}

void Interconnect::routeMessage(const CoherenceMessage& msg)
{
    switch (msg.type)
    {
        case MessageType::Writeback:
        {
            if (msg.receiver_id != MEMORY_ID && msg.receiver_id < m_caches.size())
            {
                auto copy{msg};
                copy.is_shared = true;
                copy.type = MessageType::ReadResponse;

                m_caches[msg.receiver_id]->processBusMessage(copy);
            }
            else
            {
                m_memory->processBusMessage(msg);
            }
            return;
        }

        case MessageType::WriteRequest:
        {
            auto snoop_msg{msg};
            snoop_msg.type = MessageType::SnoopInvalidate;
            
            for (std::size_t i{}; i < m_caches.size(); ++i)
            {
                if (i != msg.sender_id)
                {   
                    m_caches[i]->processBusMessage(snoop_msg);
                }
            }
            
            return;
        }

        case MessageType::ReadRequest:
        {
            auto snoop_msg{msg};
            snoop_msg.type = MessageType::SnoopRead;
            bool intercepted{};
            bool shared_wire{};

            for (std::size_t i{}; i < m_caches.size(); ++i)
            {
                if (i != msg.sender_id)
                {
                    // if a cache returns true, it fired a c2c writeback
                    if (m_caches[i]->processBusMessage(snoop_msg))
                    {
                        intercepted = true;
                    }

                    if (m_caches[i]->hasValidBlock(msg.address))
                    {
                        shared_wire = true;
                    }
                }
            }

            // only fetch from memory if no sibling cache had the dirty data
            if (!intercepted)
            {
                auto mem_msg{msg};
                mem_msg.is_shared = shared_wire;
                m_memory->processBusMessage(mem_msg);
            }
            return;
        }

        case MessageType::SnoopAck:
        {
            // route the acknowledgment directly to the cache upgrading its block
            if (msg.receiver_id < m_caches.size())
            {
                m_caches[msg.receiver_id]->processBusMessage(msg);
            }
            break;
        }

        case MessageType::ReadResponse:
        {
            if (msg.receiver_id < m_caches.size())
            {
                m_caches[msg.receiver_id]->processBusMessage(msg);
            }
            break;
        }

        case MessageType::ReadyForOwnership:
        {
            auto snoop_msg{msg};
            snoop_msg.type = MessageType::SnoopRFO;
            bool intercepted{false};

            // broadcast the RFO snoop to all sibling caches
            for (std::size_t i{}; i < m_caches.size(); ++i)
            {
                if (i != msg.sender_id)
                {
                    // if processBusMessage returns true, a sibling had dirty data and fired a Writeback
                    if (m_caches[i]->processBusMessage(snoop_msg))
                    {
                        intercepted = true;
                    }
                }
            }

            // only fetch from the MemoryController if no sibling had the dirty data
            if (!intercepted)
            {
                // send a standard ReadRequest to memory so it returns a ReadResponse payload
                auto mem_msg{msg};
                mem_msg.type = MessageType::ReadRequest; 
                m_memory->processBusMessage(mem_msg);
            }
            return;
        }
    } 
}

void Interconnect::tick() const
{}
