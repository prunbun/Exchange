#pragma once

#include "utils/macros.h"
#include "utils/logger.h"

#include "exchange/order_gateway/client_response.h"

#include "om_order.h"
// #include "risk_manager.h"

using namespace Common;

namespace Trading {

    class TradeEngine;

    class OrderManager {

        TradeEngine * trade_engine = nullptr;
        const RiskManager& risk_manager;

        std::string time_str;
        Common::Logger *logger = nullptr;

        OMOrderTickerSideHashMap ticker_order_hashmap;
        OrderId next_order_id = 1;

        public:
            OrderManager(Common::Logger *logger_param, TradeEngine *trade_engine_param, RiskManager& risk_manager_param) 
                : trade_engine(trade_engine_param), risk_manager(risk_manager_param), logger(logger_param)
            {

            }

            const OMOrderSideHashMap * getOMOrderSideHashmap(TickerId ticker_id) const {
                return &(ticker_order_hashmap.at(ticker_id));
            }

    };

}