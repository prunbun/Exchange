#pragma once

#include <functional>

#include "utils/thread_utils.h"
#include "utils/time_utils.h"
#include "utils/lock_free_queue.h"
#include "utils/macros.h"
#include "utils/logger.h"

#include "exchange/order_gateway/client_request.h"
#include "exchange/order_gateway/client_response.h"
#include "exchange/market_publisher/market_update.h"

#include "market_order_book.h"

#include "feature_engine.h"
#include "position_keeper.h"
#include "order_manager.h"
#include "risk_manager.h"

#include "market_maker.h"
#include "liquidity_taker.h"


namespace Trading {

    class TradeEngine {

        private:

            const ClientId client_id;
            MarketOrderBookHashMap ticker_order_book_hashmap;

            Exchange::ClientRequestLFQueue *outgoing_requests = nullptr;
            Exchange::ClientResponseLFQueue *incoming_responses = nullptr;
            Exchange::MEMarketUpdateLFQueue *incoming_md_updates = nullptr;

            Nanos last_event_time = 0;
            volatile bool running = false;

            std::string time_str;
            Logger logger;

            FeatureEngine feature_engine;
            PositionKeeper position_keeper;
            OrderManager order_manager;
            RiskManager risk_manager;

            MarketMaker * mm_algo = nullptr;
            LiquidityTaker *taker_algo = nullptr;


        public:
            std::function<void(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book)> algoOnOrderBookUpdate;
            std::function<void(const Exchange::MEMarketUpdate *market_update, const MarketOrderBook *book)> algoOnTradeUpdate;
            std::function<void(const Exchange::MEClientResponse *client_response)> algoOnOrderUpdate;

            void defaultAlgoOnOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book) noexcept {
                logger.log("%:% %() % TradeEngine orderbook update default - ticker:% price:% side:% \n"
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    ticker_id, Common::priceToString(price).c_str(), Common::sideToString(side).c_str()
                );
            }

            void defaultAlgoOnTradeUpdate(const Exchange::MEMarketUpdate *market_update, const MarketOrderBook *book) noexcept {
                logger.log("%:% %() % TradeEngine trade update default - % \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    market_update->toString().c_str()
                );
            }

            void defaultAlgoOnOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept {
                logger.log("%:% %() % TradeEngine order update default - % \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    client_response->toString().c_str()
                );
            }

            TradeEngine(Common::ClientId client_id_param, AlgoType algo_type, const TradeEngineConfigHashmap &ticker_cfg, 
                        Exchange::ClientRequestLFQueue *client_requests_param, Exchange::ClientResponseLFQueue *client_responses_param, Exchange::MEMarketUpdateLFQueue *market_updates_param);


    };


}