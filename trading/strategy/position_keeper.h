#pragma once

#include "utils/macros.h"
#include "utils/orderinfo_types.h"
#include "utils/logger.h"

#include "exchange/order_gateway/client_response.h"

#include "market_order_book.h"

using namespace Common;

namespace Trading {

    // struct helps us organize our positions for each asset
    // NOTE: vwap = volume weighted average price
    struct PositionInfo {
        int32_t position = 0;
        double real_pnl = 0, unreal_pnl = 0, total_pnl = 0;
        std::array<double, sideToIndex(Side::MAX) + 1> open_vwap; // product of price and execution qty on each side
        Qty volume = 0;
        const BBO *bbo = nullptr;

        std::string toString() const {
            std::stringstream ss;
            ss << "Position{"
                << " pos: " << position
                << " r-pnl: " << real_pnl
                << " u-pnl: " << unreal_pnl
                << " t-pnl: " << total_pnl
                << " vol: " << volume
                << " vwaps:[ " << (position ? open_vwap.at(sideToIndex(Side::BUY)) / std::abs(position) : 0)
                << " X " << (position ? open_vwap.at(sideToIndex(Side::SELL) / std::abs(position)) : 0)
                << " ] "
                << (bbo ? bbo->toString() : "") << "}";

            return ss.str();
        }
    };

}