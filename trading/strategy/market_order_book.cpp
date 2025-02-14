#include "market_order_book.h"

#include "trade_engine.h"


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
            START_MEASURE(Trading_MarketOrderBook_addOrder);
            addOrder(order);
            END_MEASURE(Trading_MarketOrderBook_addOrder, (*logger));
        } 
            break;

        case Exchange::MarketUpdateType::MODIFY: {
            MarketOrder * order = oid_to_order.at(market_update->order_id);
            order->qty = market_update->qty;
        }
            break;

        case Exchange::MarketUpdateType::CANCEL: {
            MarketOrder * order = oid_to_order.at(market_update->order_id);
            START_MEASURE(Trading_MarketOrderBook_removeOrder);
            removeOrder(order);
            END_MEASURE(Trading_MarketOrderBook_removeOrder, (*logger));
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
    START_MEASURE(Trading_MarketOrderBook_updateBBO);
    updateBBO(bid_updated, ask_updated);
    END_MEASURE(Trading_MarketOrderBook_updateBBO, (*logger));
    trade_engine->onOrderBookUpdate(market_update->ticker_id, market_update->price, market_update->side, this);

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

std::string Trading::MarketOrderBook::toString(bool detailed, bool validity_check) const {

    std::stringstream ss;
    std::string local_time_str;


    // printer function to print out all orders at a certain price
    auto printer = [&](std::stringstream &ss, MarketOrdersAtPrice *iter, Side side, Price &last_price, bool sanity_check) {

        char buf[4096];
        Qty qty = 0;
        size_t num_orders = 0;

        // go through each of the orders
        for (auto o_iter = iter->first_mkt_order;; o_iter = o_iter->next_order) {
            qty += o_iter->qty;
            ++num_orders;

            // if we get to the first order again, then break (note we shouldn't have empty LL)
            if (o_iter->next_order == iter->first_mkt_order) {
                break;
            }
        }

        // format print the price, qty, and num orders at this price level
        snprintf(buf, sizeof(buf), " <px:%3s prev:%3s next:%3s> %-3s @ %-5s(%-4s)", 
            priceToString(iter->price).c_str(), priceToString(iter->prev_entry->price).c_str(),
            priceToString(iter->next_entry->price).c_str(),
            priceToString(iter->price).c_str(), qtyToString(qty).c_str(), std::to_string(num_orders).c_str()
        );
        ss << buf;

        // now we print each individual order if we want a detailed breakdown 
        if (detailed) {
            for (auto o_iter = iter->first_mkt_order;; o_iter = o_iter->next_order) {
                snprintf(buf, sizeof(buf), "[oid:%s q:%s prev:%s next:%s] ",
                    orderIdToString(o_iter->order_id).c_str(), qtyToString(o_iter->qty).c_str(),
                    orderIdToString(o_iter->prev_order ? o_iter->prev_order->order_id : OrderId_INVALID).c_str(),
                    orderIdToString(o_iter->next_order ? o_iter->next_order->order_id : OrderId_INVALID).c_str()
                );
                ss << buf;

                if (o_iter->next_order == iter->first_mkt_order) {
                    break;
                }
            }
        }
        ss << std::endl;

        // let's do a sanity check to make sure that this order book is following the rules we want
        // we want each price level to be ascending for the SELL
        // and descending for the BUY
        // and we reset last_price to the current price so we can make sure that all of them follow
        // these rules
        if (sanity_check) {
            if ((side == Side::SELL && last_price >= iter->price) || (side == Side::BUY && last_price <= iter->price)) {
                FATAL("Bids/Asks not sorted by ascending/descending prices last: " + priceToString(last_price) + " itr: " + iter->toString());
                last_price = iter->price;
            }
        }
    };


    // here, we use the printer helper function print out the entire order book for this ticker
    ss << "Ticker: " << tickerIdToString(ticker_id) << std::endl;
    { // asks
        auto ask_itr = asks_by_price;
        auto last_ask_price = std::numeric_limits<Price>::min();
        for (size_t count = 0; ask_itr; ++count) {
            ss << "ASKS L:" << count << " => ";
            auto next_ask_itr = (ask_itr->next_entry == asks_by_price ? nullptr : ask_itr->next_entry);
            printer(ss, ask_itr, Side::SELL, last_ask_price, validity_check);
            ask_itr = next_ask_itr;
        }
    }
    ss << std::endl << "                          X" << std::endl << std::endl;
    { // bids
        auto bid_itr = bids_by_price;
        auto last_bid_price = std::numeric_limits<Price>::max();
        for (size_t count = 0; bid_itr; ++count) {
            ss << "BIDS L:" << count << " => ";
            auto next_bid_itr = (bid_itr->next_entry == bids_by_price ? nullptr : bid_itr->next_entry);
            printer(ss, bid_itr, Side::BUY, last_bid_price, validity_check);
            bid_itr = next_bid_itr;
        }
    }

    // conver the ss to a c++ string
    return ss.str();
}
