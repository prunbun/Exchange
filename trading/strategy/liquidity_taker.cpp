#include "liquidity_taker.h"
#include "trade_engine.h"

Trading::LiquidityTaker::LiquidityTaker(Common::Logger *logger_param, TradeEngine *trade_engine_param, 
                                        FeatureEngine *feature_engine_param, OrderManager *order_manager_param, 
                                        const TradeEngineConfigHashmap &ticker_cfg_param):
                                        feature_engine(feature_engine_param), order_manager(order_manager_param),
                                        logger(logger_param), ticker_configs_hashmap(ticker_cfg_param) {

    // here we overload the TradeEngine methods with the liquidity taker algo methods
    trade_engine_param->algoOnOrderBookUpdate = [this](TickerId ticker_id, Price price, Side side, const MarketOrderBook * book) {
        onOrderBookUpdate(ticker_id, price, side, book);
    };

    trade_engine_param->algoOnTradeUpdate = [this](const Exchange::MEMarketUpdate * market_update, const MarketOrderBook * book) {
        onTradeUpdate(market_update, book);
    };

    trade_engine_param->algoOnOrderUpdate = [this](const Exchange::MEClientResponse * client_response) {
        onOrderUpdate(client_response);
    };

}

// our liquidity taker does not react to order book updates, but rather the trades placed by other market participants
void Trading::LiquidityTaker::onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book) noexcept {
    
    logger->log("%:% %() % ticker:% price:% side:% \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        ticker_id, Common::priceToString(price).c_str(), Common::sideToString(side).c_str()
    );

}

// when the liquidity taker notices aggressive trades being made, it also places trades
void Trading::LiquidityTaker::onTradeUpdate(const Exchange::MEMarketUpdate *market_update, const MarketOrderBook *book) noexcept {

    logger->log("%:% %() % LiqTaker trade update -  % \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        market_update->toString().c_str()
    );

    const BBO * bbo = book->getBBO();
    const double aggressive_qty_ratio = feature_engine->getAggrTradeQtyRatio();

    if (LIKELY(bbo->bid_price != Price_INVALID && bbo->ask_price != Price_INVALID && aggressive_qty_ratio != Feature_INVALID)) {
        
        logger->log("%:% %() % LiqTaker BBO - % aggr-qty-ratio:% \n",
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
            bbo->toString().c_str(), aggressive_qty_ratio
        );

        const Qty trade_size = ticker_configs_hashmap.at(market_update->ticker_id).trade_size;
        const double feature_threshold = ticker_configs_hashmap.at(market_update->ticker_id).feature_threshold;

        if (aggressive_qty_ratio >= feature_threshold) {

            // we buy at the best ask price or sell at the best buy price
            if (market_update->side == Side::BUY) {
                order_manager->moveOrders(market_update->ticker_id, bbo->ask_price, Price_INVALID, trade_size);
            } else {
                order_manager->moveOrders(market_update->ticker_id, Price_INVALID, bbo->bid_price, trade_size);
            }

        }
    }
}

// once we get an order receipt from the exchange, we'll just send it to our order manager
void Trading::LiquidityTaker::onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept {

    logger->log("%:% %() % LiqTaker forwarding order update - % \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        client_response->toString().c_str()
    );
    order_manager->onOrderUpdate(client_response);
}
