#include "market_order_book.h"

Trading::MarketOrderBook::MarketOrderBook(TickerId ticker_id_param, Logger *logger_param): 
                                        ticker_id(ticker_id_param),
                                        orders_at_price_pool(ME_MAX_PRICE_LEVELS),
                                        order_pool(ME_MAX_ORDER_IDs),
                                        logger(logger_param) {

}

Trading::MarketOrderBook::~MarketOrderBook() {
    logger->log("%:% %() % ~MarketOrderBook() \n % \n", 
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        toString(false, true)
    );

    trade_engine = nullptr;
    bids_by_price = asks_by_price = nullptr;
    oid_to_order.fill(nullptr);
}
