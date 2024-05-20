#pragma once

#include <sstream>

#include "../../utils/orderinfo_types.h"
#include "../../utils/lock_free_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1)

    // here, we define the objects that the matching engine will use to give a status update on orders to clients

    enum class ClientResponseType : uint8_t {
        INVALID = 0,
        ACCEPTED = 1,
        CANCELED = 2,
        FILLED = 3,
        CANCEL_REJECTED = 4
    };

    inline std::string clientResponseTypeToString(ClientResponseType type) {
        switch (type) {
            case ClientResponseType::INVALID:
                return "INVALID";
            case ClientResponseType::ACCEPTED:
                return "ACCEPTED";
            case ClientResponseType::CANCELED:
                return "CANCELED";
            case ClientResponseType::FILLED:
                return "FILLED";
            case ClientResponseType::CANCEL_REJECTED:
                return "CANCEL_REJECTED";
        }

        return "UNKNOWN";
    }

    struct MEClientResponse {
        ClientResponseType type = ClientResponseType::INVALID;
        ClientId client_id = ClientId_INVALID;
        TickerId ticker_id = TickerId_INVALID;
        OrderId client_order_id = OrderId_INVALID; // unique per client, may not be unqiue in the market
        OrderId market_order_id = OrderId_INVALID; // unique identifier given to this order in the market
        Side side = Side::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Qty exec_qty = Qty_INVALID; // if applicable, what quantity of the order was executed
        Qty leaves_qty = Qty_INVALID; // what quantity of the original order is still live in the order book

        std::string toString() const {
            std::stringstream ss;
            
            ss << "MEClientResponse ["
            << "type: " << clientResponseTypeToString(type)
            << ", client: " << clientIdToString(client_id)
            << ", ticker: " << tickerIdToString(ticker_id) 
            << ", client order id: " << orderIdToString(client_order_id)
            << ", market order id: " << orderIdToString(market_order_id)
            << ", side: " << sideToString(side)
            << ", exec_qty: " << qtyToString(exec_qty)
            << ", leaves_qty: " << qtyToString(leaves_qty)
            << ", price: " << priceToString(price)
            << "]";

            return ss.str();
        }
    };

#pragma pack(pop)

    // queue for the engine to send status updates of orders to clients
    typedef LFQUEUE<MEClientResponse> ClientResponseLFQueue;
}