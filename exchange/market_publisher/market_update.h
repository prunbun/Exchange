#pragma once

#include <sstream>

#include "../../utils/orderinfo_types.h"
#include "../../utils/lock_free_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1) // ensures that we don't introduce padding for alignment, drawbacks include compiler variability and additional overhead for accessing at an offset

    // this file defines objects used by the matching engine to send order updates to be published to the market
    enum class MarketUpdateType : uint8_t {
        INVALID = 0,
        CLEAR = 1,
        ADD = 2,
        MODIFY = 3,
        CANCEL = 4,
        TRADE = 5,
        SNAPSHOT_START = 6,
        SNAPSHOT_END = 7
    };

    inline std::string marketUpdateTypeToString(MarketUpdateType type) {
        switch (type) {
            case MarketUpdateType::INVALID:
                return "INVALID";
            case MarketUpdateType::CLEAR:
                return "CLEAR";
            case MarketUpdateType::ADD:
                return "ADD";
            case MarketUpdateType::MODIFY:
                return "MODIFY";
            case MarketUpdateType::CANCEL:
                return "CANCEL";
            case MarketUpdateType::TRADE:
                return "TRADE";
            case MarketUpdateType::SNAPSHOT_START:
                return "SNAPSHOT_START";
            case MarketUpdateType::SNAPSHOT_END:
                return "SNAPSHOT_END";
        }

        return "UNKNOWN";
    }

    struct MEMarketUpdate {
        MarketUpdateType type = MarketUpdateType::INVALID;

        // order details, notice how we do not include client id as that would be a data leak
        OrderId order_id = OrderId_INVALID;
        TickerId ticker_id = TickerId_INVALID;
        Side side = Side::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;

        std::string toString() const {
            std::stringstream ss;

            ss << "MEMarketUpdate ["
            << "type: " << marketUpdateTypeToString(type)
            << ", order_id: " << orderIdToString(order_id)
            << ", ticker: " << tickerIdToString(ticker_id)
            << ", side: " << sideToString(side)
            << ", price: " << priceToString(price)
            << ", qty: " << qtyToString(qty)
            << ", priority: " << priorityToString(priority)
            << "]";

            return ss.str();
        }
    };

    // "Market Data Protocol", difference is that it has a seq. number that clients can check for message drops
    // This is the actual format that goes out to clients
    struct MDPMarketUpdate {
        size_t seq_number = 0;
        MEMarketUpdate me_market_update;

        std::string toString() const {
            std::stringstream ss;
            
            ss << "MDPMarketUpdate ["
            << " seq: " << seq_number
            << " " << me_market_update.toString()
            << "]";

            return ss.str();
        }
    };

#pragma pack(pop) // puts our configuration back

    // queue for the engine to send status updates of orders to the market
    typedef LFQUEUE<MEMarketUpdate> MEMarketUpdateLFQueue;
    typedef LFQUEUE<MDPMarketUpdate> MDPMarketUpdateLFQueue;
}