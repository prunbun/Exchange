#pragma once

#include "utils/orderinfo_types.h"
#include "utils/exchange_limits.h"

#include <sstream>


using namespace Common;

namespace Trading {

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

