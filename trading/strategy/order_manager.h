#pragma once

#include "utils/macros.h"
#include "utils/logger.h"

#include "exchange/order_gateway/client_response.h"
#include "exchange/order_gateway/client_request.h"

#include "om_order.h"
// #include "risk_manager.h"

using namespace Common;

namespace Trading {

    class TradeEngine;

    class OrderManager {

        private:
            TradeEngine * trade_engine = nullptr;
            const RiskManager& risk_manager;

            std::string time_str;
            Common::Logger *logger = nullptr;

            OMOrderTickerSideHashMap ticker_order_hashmap;
            OrderId next_order_id = 1;
            
            // lower-level private methods for order management
            void newOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept;
            void cancelOrder(OMOrder *order) noexcept;
            void moveOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept;

        public:
            OrderManager(Common::Logger *logger_param, TradeEngine *trade_engine_param, RiskManager& risk_manager_param) 
                : trade_engine(trade_engine_param), risk_manager(risk_manager_param), logger(logger_param)
            {

            }

            const OMOrderSideHashMap * getOMOrderSideHashmap(TickerId ticker_id) const {
                return &(ticker_order_hashmap.at(ticker_id));
            }


            // 
            void moveOrders(TickerId ticker_id, Price bid_price, Price ask_price, Qty trade_size) noexcept {



            }


            

    };

}