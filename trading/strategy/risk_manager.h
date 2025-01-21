#pragma once

#include <sstream>

#include "utils/macros.h"
#include "utils/logger.h"

#include "utils/exchange_limits.h"
#include "position_keeper.h"
#include "om_order.h"


using namespace Common;

namespace Trading {

    class OrderManager;

    enum class RiskCheckResult : int8_t {
        INVALID = 0,
        ORDER_TOO_LARGE = 1,
        POSITION_TOO_LARGE = 2,
        LOSS_TOO_LARGE = 3,
        ALLOWED = 4
    };

    inline std::string riskCheckResultToString(RiskCheckResult result) {

        switch (result) {

            case RiskCheckResult::INVALID: {return "INVALID";}
            case RiskCheckResult::ORDER_TOO_LARGE: {return "ORDER_TOO_LARGE";}
            case RiskCheckResult::POSITION_TOO_LARGE: {return "POSITION_TOO_LARGE";}
            case RiskCheckResult::LOSS_TOO_LARGE: {return "LOSS_TOO_LARGE";}
            case RiskCheckResult::ALLOWED: {return "ALLOWED";}

            return "";
        }

    }

    struct RiskInfo {

        const PositionInfo *position_info = nullptr;
        RiskConfig risk_cfg;

        std::string toString() const  {
            std::stringstream ss;

            ss << "RiskInfo" << "[ "
                << "pos: " << position_info->toString() << " "
                << risk_cfg.toString() 
                << "]";

            return ss.str();
        }

        RiskCheckResult checkPreTradeRisk(Side side, Qty qty) const noexcept {

            if (UNLIKELY(qty > risk_cfg.max_order_size)) {
                return RiskCheckResult::ORDER_TOO_LARGE;
            }

            if (UNLIKELY(std::abs(position_info->position + sideToValue(side) * static_cast<int32_t>(qty)) > static_cast<int32_t>(risk_cfg.max_position))) {
                return RiskCheckResult::POSITION_TOO_LARGE;
            }

            if (UNLIKELY(position_info->total_pnl < risk_cfg.max_loss)) {
                return RiskCheckResult::LOSS_TOO_LARGE;
            }

            return RiskCheckResult::ALLOWED;
        }
    };

    typedef std::array<RiskInfo, ME_MAX_TICKERS> TickerRiskInfoHashMap;


    class RiskManager {

        private:
            std::string time_str;
            Common::Logger *logger = nullptr;

            TickerRiskInfoHashMap ticker_risk_hashmap;

        public:
            RiskManager(Common::Logger * logger_param, const PositionKeeper *position_keeper_param, const TradeEngineConfigHashmap &ticker_config_param);

            RiskCheckResult checkPreTradeRisk(TickerId ticker_id, Side side, Qty qty) const noexcept {
                return ticker_risk_hashmap.at(ticker_id).checkPreTradeRisk(side, qty);
            }
    };

}

