#include "trade_engine.h"

Trading::TradeEngine::TradeEngine(Common::ClientId client_id_param, AlgoType algo_type, const TradeEngineConfigHashmap &ticker_cfg, 
                                Exchange::ClientRequestLFQueue *client_requests_param, Exchange::ClientResponseLFQueue *client_responses_param, 
                                Exchange::MEMarketUpdateLFQueue *market_updates_param):
                                client_id(client_id_param), outgoing_requests(client_requests_param),
                                incoming_responses(client_responses_param), incoming_md_updates(market_updates_param),
                                logger("trading_engine" + std::to_string(client_id) + ".log"),
                                feature_engine(&logger), position_keeper(&logger), order_manager(&logger, this, risk_manager),
                                risk_manager(&logger, &position_keeper, ticker_cfg)
{

    // initialize our order book
    for (size_t i = 0; i < ticker_order_book_hashmap.size(); ++i) {
        ticker_order_book_hashmap[i] = new MarketOrderBook(i, &logger);
        ticker_order_book_hashmap[i]->setTradingEngine(this);
    }

    // set defaults, although a strategy should override these
    algoOnOrderBookUpdate = [this](TickerId ticker_id, Price price, Side side, const MarketOrderBook * book) {
        defaultAlgoOnOrderBookUpdate(ticker_id, price, side, book);
    };
    algoOnTradeUpdate = [this](const Exchange::MEMarketUpdate * market_update, const MarketOrderBook * book) {
        defaultAlgoOnTradeUpdate(market_update, book);
    };
    algoOnOrderUpdate = [this](const Exchange::MEClientResponse * client_response) {
        defaultAlgoOnOrderUpdate(client_response);
    };

    // set this client's strategy, note that we can make our own strategy and simply replace it here
    if (algo_type == AlgoType::MAKER) {
        mm_algo = new MarketMaker(&logger, this, &feature_engine, &order_manager, ticker_cfg);
    } else if (algo_type == AlgoType::TAKER) {
        taker_algo = new LiquidityTaker(&logger, this, &feature_engine, &order_manager, ticker_cfg);
    }

    // now let's log the starting state of the trade engine configuration for each instrument
    for (TickerId i = 0; i < ticker_cfg.size(); ++i) {
        logger.log("%:% %() % TradeEngine Configs Initialized % Ticker: % %. \n",
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
            algoTypeToString(algo_type), i, ticker_cfg.at(i).toString()
        );
    }
}

Trading::TradeEngine::~TradeEngine() {

    running = false;

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);

    delete mm_algo; mm_algo = nullptr;
    delete taker_algo; taker_algo = nullptr;

    for (auto &order_book: ticker_order_book_hashmap) {
        delete order_book;
        order_book = nullptr;
    }

    outgoing_requests = nullptr;
    incoming_responses = nullptr;
    incoming_md_updates = nullptr;
}

void Trading::TradeEngine::sendClientRequest(const Exchange::MEClientRequest *client_request) noexcept {
    logger.log("%:% %() % TradeEngine Sending %\n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        client_request->toString().c_str()
    );

    Exchange::MEClientRequest * next_write = outgoing_requests->getNextWriteTo();
    *next_write = std::move(*client_request);
    outgoing_requests->updateWriteIndex();
}

void Trading::TradeEngine::run() noexcept {

    logger.log("%:% %() % TradeEngine running! \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
    );

    while (running) {

        // process incoming receipts from the exchange
        for (const Exchange::MEClientResponse *client_response = incoming_responses->getNextRead(); client_response; client_response = incoming_responses->getNextRead()) {

            logger.log("%:% %() % Processing Exchange Receipt: %\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                client_response->toString().c_str()
            );

            onOrderUpdate(client_response);
            incoming_responses->updateReadIndex();
            last_event_time = Common::getCurrentNanos();
        }

        // process all market data updates
        for (const Exchange::MEMarketUpdate *market_update = incoming_md_updates->getNextRead(); market_update; market_update = incoming_md_updates->getNextRead()) {

            logger.log("%:% %() % Processing Market Update % \n"
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                market_update->toString().c_str()
            );

            ASSERT(market_update->ticker_id < ticker_order_book_hashmap.size(), "Unkown ticker-id on update:" + market_update->toString());
            ticker_order_book_hashmap[market_update->ticker_id]->onMarketUpdate(market_update);

            incoming_md_updates->updateReadIndex();
            last_event_time = Common::getCurrentNanos();
        }
    }

}

void Trading::TradeEngine::onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book) noexcept {

    logger.log("%:% %() % TradeEngine reaction to orderbook change - ticker: % price: % side: % \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        ticker_id, Common::priceToString(price).c_str(), Common::sideToString(side).c_str()
    );

    // if a market update causes the order book for this instrument to change, we will recalculate our 
    // current positon and PnL, recalculate features, and have the algorithm 'react'
    const BBO * bbo = book->getBBO();
    position_keeper.updateBBO(ticker_id, bbo);
    feature_engine.onOrderBookUpdate(ticker_id, price, side, book);
    algoOnOrderBookUpdate(ticker_id, price, side, book);
}

void Trading::TradeEngine::onTradeUpdate(const Exchange::MEMarketUpdate *market_update, const MarketOrderBook *book) noexcept {

    logger.log("%:% %() % TradeEngine reaction to trade - %\n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        market_update->toString().c_str()
    );

    feature_engine.onTradeUpdate(market_update, book);
    algoOnTradeUpdate(market_update, book);
}

void Trading::TradeEngine::onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept {

    logger.log("%:% %() % TradeEngine reaction to order update - % \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        client_response->toString().c_str()
    );

    // we'll update our PnL if an order has successfully gone through and we'll notify the algorithm of what happened
    if (UNLIKELY(client_response->type == Exchange::ClientResponseType::FILLED)) {
        position_keeper.addFill(client_response);
    }

    algoOnOrderUpdate(client_response);
}
