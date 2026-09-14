#include "Interconnect.h"
#include "Cache.h"

void Interconnect::attachCache(Cache* cache)
{
    m_caches.push_back(cache);
}

void Interconnect::routeMessage(const CoherenceMessage& msg)
{
    switch (msg.type)
    {
        case MessageType::Writeback:
        {
            assert(msg.receiver_id < m_caches.size());

            auto copy{msg};
            copy.type = MessageType::ReadResponse;

            m_caches[msg.receiver_id]->processBusMessage(copy);
            
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
            bool intercepted{false};

            for (std::size_t i{}; i < m_caches.size(); ++i)
            {
                if (i != msg.sender_id)
                {
                    // if a cache returns true, it fired a c2c writeback
                    if (m_caches[i]->processBusMessage(snoop_msg))
                    {
                        intercepted = true;
                    }
                }
            }

            // only fetch from memory if no sibling cache had the dirty data
            if (!intercepted)
            {
                CoherenceMessage mock_message{MessageType::ReadResponse, msg.address, msg.transaction_id, msg.sender_id, msg.sender_id, {}};
                m_caches[msg.sender_id]->processBusMessage(mock_message);
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
    } 
}

