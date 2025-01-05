#pragma once

#include <array>
#include <sstream>
#include "utils/orderinfo_types.h"
#include "utils/exchange_limits.h"

using namespace Common;

namespace Trading {

    // enum for tracking order state
    enum class OMOrderState: int8_t {
        INVALID = 0,
        PENDING_NEW = 1, // order sent, no receipt yet
        LIVE = 2, // in the exchange
        PENDING_CANCEL = 3, // cancel sent, no receipt yet
        DEAD = 4 // completed | executed | cancelled
    };

    inline std::string OMOrderStateToString(OMOrderState state) {
        switch (state) {
            case OMOrderState::PENDING_NEW:
                return "PENDING_NEW";
            case OMOrderState::LIVE:
                return "LIVE";
            case OMOrderState::PENDING_CANCEL:
                return "PENDING_CANCEL";
            case OMOrderState::DEAD:
                return "DEAD";
            case OMOrderState::INVALID:
                return "INVALID";
        }

        return "UNKNOWN";
    }

    struct OMOrder {

        TickerId ticker_id = TickerId_INVALID;
        OrderId order_id = OrderId_INVALID;
        Side side = Side::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        OMOrderState order_state = OMOrderState::INVALID;

        std::string toString() const {
            std::stringstream ss;

            ss << "OMOrder " << "[ "
                << "ticker id: " << tickerIdToString(ticker_id) << " "
                << "order id: " << orderIdToString(order_id) << " "
                << "side: " << sideToString(side) << " "
                << "price: " << priceToString(price) << " "
                << "qty: " << qtyToString(qty) << " "
                << "state: " << OMOrderStateToString(order_state)
                << " ]";

            return ss.str();
        }

    };

    // each ticker can have up to 1 order per side
    typedef std::array<OMOrder, sideToIndex(Side::MAX) + 1> OMOrderSideHashMap;

    // we store up to 1 buy order and 1 sell order per ticker
    typedef std::array<OMOrderSideHashMap, ME_MAX_TICKERS> OMOrderTickerSideHashMap;

}