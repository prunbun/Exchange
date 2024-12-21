#pragma once

#include "../../utils/orderinfo_types.h"
#include "../../utils/memory_pool.h"
#include "../../utils/logger.h"

#include "market_order.h"
#include "exchange/market_publisher/market_update.h"

namespace Trading {

    class TradeEngine; // forward declaration

    class MarketOrderBook final {
        
        private:
            // each order book is built for a particular ticker/security
            const TickerId ticker_id;

            // instead of a matching engine, we have a trading engine
            TradeEngine * trade_engine = nullptr;

            OrderHashMap oid_to_order;

            // if we need the nodes to persist after a function ends, they have to go onto the heap
            // here, it is the mempool
            MemPool<MarketOrdersAtPrice> orders_at_price_pool;

            // best offers for buy and sell
            MarketOrdersAtPrice *bids_by_price = nullptr;
            MarketOrdersAtPrice *asks_by_price = nullptr;

            MemPool<MarketOrder> order_pool;

            BBO bbo;

            std::string time_str;
            Logger *logger = nullptr;

        public:
            MarketOrderBook(TickerId ticker_id_param, Logger *logger_param);
            ~MarketOrderBook();

            auto setTradingEngine(TradeEngine *trade_engine_param) {
                trade_engine = trade_engine_param;
            }

    };

    typedef std::array<MarketOrderBook *, ME_MAX_TICKERS> MarketOrderBookHashMap;

}