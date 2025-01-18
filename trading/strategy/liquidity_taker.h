#pragma once

#include "utils/macros.h"
#include "utils/logger.h"

#include "order_manager.h"
#include "feature_engine.h"

using namespace Common;

namespace Trading {

    class LiquidityTaker {

        private:
            const FeatureEngine *feature_engine = nullptr;
            OrderManager *order_manager = nullptr;

            std::string time_str;
            Common::Logger *logger = nullptr;

            const TradeEngineConfigHashmap ticker_configs_hashmap;

        public:
            LiquidityTaker(Common::Logger * logger_param, TradeEngine *trade_engine_param, 
                            FeatureEngine * feature_engine_param, OrderManager * order_manager_param, TradeEngineConfigHashmap &ticker_cfg_param);
            
            void onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook * book) noexcept;
            void onTradeUpdate(const Exchange::MEMarketUpdate * market_update, const MarketOrderBook *book) noexcept;
            void onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept;

    };

}