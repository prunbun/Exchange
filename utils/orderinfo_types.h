#pragma once

#include <cstdint>
#include <limits>
#include "utils/macros.h"
#include "utils/exchange_limits.h"

namespace Common {
    // order id
    typedef uint64_t OrderId;
    constexpr auto OrderId_INVALID = std::numeric_limits<OrderId>::max();

    inline std::string orderIdToString(OrderId order_id) {
        if (UNLIKELY(order_id == OrderId_INVALID)) {
            return "INVALID";
        }

        return std::to_string(order_id);
    }

    // ticker id
    typedef uint32_t TickerId;
    constexpr auto TickerId_INVALID = std::numeric_limits<TickerId>::max();

    inline std::string tickerIdToString(TickerId ticker_id) {
        if (UNLIKELY(ticker_id == TickerId_INVALID)) {
            return "INVALID";
        }

        return std::to_string(ticker_id);
    }

    // client id
    typedef uint32_t ClientId;
    constexpr auto ClientId_INVALID = std::numeric_limits<ClientId>::max();

    inline std::string clientIdToString(ClientId client_id) {
        if (UNLIKELY(client_id == ClientId_INVALID)) {
            return "INVALID";
        }

        return std::to_string(client_id);
    }

    // price
    typedef int64_t Price;
    constexpr auto Price_INVALID = std::numeric_limits<Price>::max();

    inline std::string priceToString(Price price) {
        if (UNLIKELY(price == Price_INVALID)) {
            return "INVALID";
        }

        return std::to_string(price);
    }

    // quantity
    typedef uint32_t Qty;
    constexpr auto Qty_INVALID = std::numeric_limits<Qty>::max();

    inline std::string qtyToString(Qty qty) {
        if (UNLIKELY(qty == Qty_INVALID)) {
            return "INVALID";
        }

        return std::to_string(qty);
    }

    // priority (position in the order book queue)
    typedef uint64_t Priority;
    constexpr auto Priority_INVALID = std::numeric_limits<Priority>::max();

    inline std::string priorityToString(Priority priority) {
        if (UNLIKELY(priority == Priority_INVALID)) {
            return "INVALID";
        }

        return std::to_string(priority);
    }


    // side of the order book
    enum class Side : int8_t {
        BUY = 1,
        INVALID = 0,
        SELL = -1,
        MAX = 2
    };

    inline std::string sideToString(Side side) {
        switch (side) {
            case Side::BUY:
                return "BUY";
            case Side::INVALID:
                return "INVALID";
            case Side::SELL:
                return "SELL";
            case Side::MAX:
                return "MAX";
        }

        return "UNKNOWN";
    }

    inline constexpr auto sideToIndex(Side side) noexcept {
        return static_cast<size_t>(side) + 1;
    }

    inline constexpr int sideToValue(Side side) noexcept {
        return static_cast<int>(side);
    }

    // each ticker will have a risk configuration associated with it
    struct RiskConfig {
        Qty max_order_size = 0;
        Qty max_position = 0;
        double max_loss = 0;

        std::string toString() const {
            std::stringstream ss;

            ss << "RiskConfig { "
                << "max-order-size: " << qtyToString(max_order_size) << " "
                << "max-position: " << qtyToString(max_position) << " "
                << "max-loss: " << max_loss 
                << " }";

            return ss.str();
        }
    };

    struct TradeEngineConfig {
        Qty trade_size = 0;
        double feature_threshold = 0;
        RiskConfig risk_cfg;

        std::string toString() const {
            std::stringstream ss;

            ss << "TradeEngineConfig { "
                << "trade-size: " << qtyToString(trade_size) << " "
                << "feature-threshold: " << feature_threshold << " "
                << "risk config: " << risk_cfg.toString() << " "
                << "}";

            return ss.str();
        }
    };

    typedef std::array<TradeEngineConfig, ME_MAX_TICKERS> TradeEngineConfigHashmap;
}