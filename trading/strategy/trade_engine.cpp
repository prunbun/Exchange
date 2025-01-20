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