#include "MemoryController.h"
#include "Interconnect.h"

void MemoryController::processBusMessage(const CoherenceMessage& msg)
{
    switch (msg.type)
    {
        case MessageType::ReadRequest:
        {
            PendingRequest pending_request{msg, m_current_cycle+m_memory_latency};

            m_pending_requests.push(pending_request);

            return;
        }

        case MessageType::Writeback:
        {
            m_backing_store[msg.address] = msg.data; // insert/overwrite data

            return;
        }
    }
}

void MemoryController::tick()
{
    ++m_current_cycle;

    if (m_pending_requests.empty()) 
    {
        return;
    }

    auto request{m_pending_requests.front()};

    if (request.resolve_cycle == m_current_cycle)
    {
        auto msg{request.msg};

        m_pending_requests.pop();

        auto it{m_backing_store.find(msg.address)}; // block address

        CoherenceMessage read_response{MessageType::ReadResponse, msg.address, msg.transaction_id, msg.sender_id, msg.sender_id};
        read_response.is_shared = msg.is_shared;

        if (it != m_backing_store.end()) // address is in memory
        {
            read_response.data = it->second;
        }

        m_interconnect->routeMessage(read_response);
    }
}