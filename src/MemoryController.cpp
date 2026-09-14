#include "MemoryController.h"
#include "Interconnect.h"

void MemoryController::processBusMessage(const CoherenceMessage& msg)
{
    switch (msg.type)
    {
        case MessageType::ReadRequest:
        {
            auto it{m_backing_store.find(msg.address)}; // block address

            CoherenceMessage read_response{MessageType::ReadResponse, msg.address, msg.transaction_id, msg.sender_id, msg.sender_id};

            if (it != m_backing_store.end()) // address is in memory
            {
                read_response.data = it->second;
            }

            m_interconnect->routeMessage(read_response);

            return;
        }

        case MessageType::Writeback:
        {
            m_backing_store[msg.address] = msg.data; // insert/overwrite data

            return;
        }
    }
}