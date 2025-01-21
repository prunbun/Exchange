#include "market_maker.h"
#include "trade_engine.h"

Trading::MarketMaker::MarketMaker(Common::Logger *logger_param, TradeEngine *trade_engine_param, const FeatureEngine *feature_engine_param,
                        OrderManager *order_manager_param, const TradeEngineConfigHashmap &ticker_cgf_param) :
                        feature_engine(feature_engine_param), order_manager(order_manager_param), logger(logger_param), ticker_configs_hashmap(ticker_cgf_param)
{

    // we will set the trade engine's callbacks to call the methods in this strategy
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

void Trading::MarketMaker::onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book) noexcept {

    logger->log("%:% %() % ticker:% price:% side:% \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        ticker_id, Common::priceToString(price).c_str(), Common::sideToString(side).c_str()
    );

    // when the order book changes, algo will adjust its bid and ask prices
    const BBO * bbo = book->getBBO();
    const auto fair_price = feature_engine->getMktPrice();

    if (LIKELY(bbo->bid_price != Price_INVALID && bbo->ask_price != Price_INVALID && fair_price != Feature_INVALID)) {
        logger->log("%:% %() % % fair-price:% \n",
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
            bbo->toString().c_str(), fair_price
        );

        const Qty trade_size = ticker_configs_hashmap.at(ticker_id).trade_size;
        const double feature_threshold = ticker_configs_hashmap.at(ticker_id).feature_threshold;

        // if we think the fair market price is higher than the current price, we are willing to buy at the best price
        // else, we will buy at a lower price
        // vice versa with selling, unless we are getting a good deal, we will only sell at a higher price
        const auto bid_price = bbo->bid_price - (fair_price - bbo->bid_price >= feature_threshold ? 0 : 1);
        const auto ask_price = bbo->ask_price + (bbo->ask_price - fair_price >= feature_threshold ? 0 : 1);

        order_manager->moveOrders(ticker_id, bid_price, ask_price, trade_size);
    }

}

// our market maker is not really affected by trades, only the fair market value from the order book changes
// so we'll just write an acknowledgement log here
void Trading::MarketMaker::onTradeUpdate(const Exchange::MEMarketUpdate *market_update, const MarketOrderBook *book) noexcept {

    logger->log("%:% %() % MM trade ACK % \n"
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        market_update->toString().c_str()
    );

}

// once we get an order receipt from the exchange, we'll just send it to our order manager
void Trading::MarketMaker::onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept {

    logger->log("%:% %() % MM forwarding order update - % \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        client_response->toString().c_str()
    );
    order_manager->onOrderUpdate(client_response);
}
