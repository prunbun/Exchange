#pragma once

#include <sstream>

#include "orderinfo_types.h"
#include "../utils/lock_free_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1) // this line makes sure there is no extra padding in the structures
    
    // here, we will define object types that the matching engine will use to process incoming orders
    enum class ClientRequestType : uint8_t {
        INVALID = 0,
        NEW = 1,
        CANCEL = 2
    };

    inline std::string clientRequestTypeToString(ClientRequestType type) {
        switch (type) {
            case ClientRequestType::INVALID:
                return "INVALID";
            case ClientRequestType::NEW:
                return "NEW";
            case ClientRequestType::CANCEL:
                return "CANCEL";
        }

        return "UNKNOWN";
    }

    struct MEClientRequest {
        ClientRequestType type = ClientRequestType::INVALID;

        // order details
        ClientId client_id = ClientId_INVALID;
        TickerId ticker_id = TickerId_INVALID;
        OrderId order_id = OrderId_INVALID;
        Side side = Side::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;

        std::string toString() const {
            std::stringstream ss; // similar to Java Stringbuilder

            ss << "MEClientRequest ["
            << "type: " << clientRequestTypeToString(type)
            << ", client: " << clientIdToString(client_id)
            << ", ticker: " << tickerIdToString(ticker_id)
            << ", order_id: " << orderIdToString(order_id)
            << ", side: " << sideToString(side)
            << ", qty: " << qtyToString(qty)
            << ", price: " << priceToString(price)
            << "]";

            return ss.str();
        }
    };

#pragma pack(pop) // undoing the previous #pragma command, we only want to pack structs that will go on the network
    
    // queue for the engine to process orders and update the order book
    typedef LFQUEUE<MEClientRequest> ClientRequestLFQueue;
}