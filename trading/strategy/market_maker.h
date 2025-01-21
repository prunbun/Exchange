#pragma once

#include "utils/logger.h"
#include "utils/macros.h"

#include "order_manager.h"
#include "feature_engine.h"

using namespace Common;

namespace Trading {

    class MarketMaker {

        private:
            const FeatureEngine *feature_engine = nullptr; // extracting signals from the feed
            OrderManager *order_manager = nullptr; // places and updates order state

            std::string time_str;
            Common::Logger *logger = nullptr;

            // every ticker has its own config
            const TradeEngineConfigHashmap ticker_configs_hashmap; // useful for placing trades and checking risk

        public:

            MarketMaker(Common::Logger *logger_param, TradeEngine *trade_engine_param, const FeatureEngine *feature_engine_param,
                        OrderManager *order_manager_param, const TradeEngineConfigHashmap &ticker_cgf_param);

            MarketMaker() = delete;
            MarketMaker(const MarketMaker &) = delete;
            MarketMaker(const MarketMaker &&) = delete;
            MarketMaker &operator=(const MarketMaker &) = delete;
            MarketMaker &operator=(const MarketMaker &&) = delete;

            void onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book) noexcept;
            void onTradeUpdate(const Exchange::MEMarketUpdate *market_update, const MarketOrderBook * book) noexcept;
            void onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept;

    };


}