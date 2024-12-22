#include "market_order_book.h"
// #include "trade_engine.h"

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

// Called to process update from market data consumer for this order book
void Trading::MarketOrderBook::onMarketUpdate(const Exchange::MEMarketUpdate *market_update) noexcept {

    // first, we check if this might update our BBO
    bool bid_updated = (bids_by_price) && (market_update->side == Side::BUY) && (market_update->price >= bids_by_price->price);
    bool ask_updated = (asks_by_price) && (market_update->side == Side::SELL) && (market_update->price <= asks_by_price->price);

    // now, let's update our order book depending on what type of update it is
    // note, if it is an update type that does not require orderbook changes, then we can return
    switch (market_update->type) {
        
        case Exchange::MarketUpdateType::ADD: {
            MarketOrder * order = order_pool.allocate(market_update->order_id, 
                                                market_update->side, 
                                                market_update->price, 
                                                market_update->qty,
                                                market_update->priority,
                                                nullptr,
                                                nullptr    
                                            );
            addOrder(order);
        } 
            break;

        case Exchange::MarketUpdateType::MODIFY: {
            MarketOrder * order = oid_to_order.at(market_update->order_id);
            order->qty = market_update->qty;
        }
            break;

        case Exchange::MarketUpdateType::CANCEL: {
            MarketOrder * order = oid_to_order.at(market_update->order_id);
            removeOrder(order);
        }
            break;

        case Exchange::MarketUpdateType::TRADE: {
            trade_engine->onTradeUpdate(market_update, this);
            return;
        }
            break;

        case Exchange::MarketUpdateType::CLEAR: {
            // here, we are essentially wiping the order book and are going to rebuild it
            
            for (MarketOrder * &order : oid_to_order) {
                if (order) {
                    order_pool.deallocate(order);
                }
            }
            oid_to_order.fill(nullptr);

            if(bids_by_price) {
                for (auto bid = bids_by_price->next_entry; bid != bids_by_price; bid = bid->next_entry) {
                    orders_at_price_pool.deallocate(bid);
                }
                orders_at_price_pool.deallocate(bids_by_price);
            }

            if(asks_by_price) {
                for (auto ask = asks_by_price->next_entry; ask != asks_by_price; ask = ask->next_entry) {
                    orders_at_price_pool.deallocate(asks_by_price);
                }
                orders_at_price_pool.deallocate(asks_by_price);
            }

            bids_by_price = asks_by_price = nullptr;

        }
            break;

        // these are cases only the market data consumer will handle
        case Exchange::MarketUpdateType::INVALID:
        case Exchange::MarketUpdateType::SNAPSHOT_START:
        case Exchange::MarketUpdateType::SNAPSHOT_END:
            break;
    }

    // cool! now our order book has been updated with the proper methods
    // we can now ask the trading engine to 'react' to these updates
    updateBBO(bid_updated, ask_updated);
    trade_engine->onOrderBookUpdate(market_update->ticker_id, market_update->price, market_update->side);

    logger->log("%:% %() % OrderBook \n % \n", 
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        toString(false, true)
    );
}

void Trading::MarketOrderBook::updateBBO(bool update_bid, bool update_ask) noexcept {
    
    if (update_bid) {

        if (bids_by_price) {
            bbo.bid_price = bids_by_price->price;
            bbo.bid_qty = bids_by_price->first_mkt_order->qty;

            for (auto order = bids_by_price->first_mkt_order->next_order; order != bids_by_price->first_mkt_order; order = order->next_order) {
                bbo.bid_qty += order->qty;
            }
        } else {
            bbo.bid_price = Price_INVALID;
            bbo.bid_qty = Qty_INVALID;
        }

    }

    if(update_ask) {
        
        if (asks_by_price) {
            bbo.ask_price = asks_by_price->price;
            bbo.ask_qty = asks_by_price->first_mkt_order->qty;

            for (auto order = asks_by_price->first_mkt_order->next_order; order != asks_by_price->first_mkt_order; order = order->next_order) {
                bbo.ask_qty += order->qty;
            }
        } else {
            bbo.ask_price = Price_INVALID;
            bbo.ask_qty = Qty_INVALID;
        }

    }

    return;

}
