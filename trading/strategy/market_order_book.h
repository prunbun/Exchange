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
            OrdersAtPriceHashmap price_orders_at_price;

            // if we need the nodes to persist after a function ends, they have to go onto the heap
            // here, it is the mempool
            MemPool<MarketOrdersAtPrice> orders_at_price_pool;
            MemPool<MarketOrder> order_pool;

            // best offers for buy and sell
            MarketOrdersAtPrice *bids_by_price = nullptr;
            MarketOrdersAtPrice *asks_by_price = nullptr;



            BBO bbo;

            std::string time_str;
            Logger *logger = nullptr;

        public:
            MarketOrderBook(TickerId ticker_id_param, Logger *logger_param);
            ~MarketOrderBook();

            auto setTradingEngine(TradeEngine *trade_engine_param) {
                trade_engine = trade_engine_param;
            }

            // method that is called with every processed update from market data consumer
            void onMarketUpdate(const Exchange::MEMarketUpdate *market_update) noexcept;

            void updateBBO(bool update_bid, bool update_ask) noexcept;

            std::string toString(bool detailed, bool validity_check) const;

        private:
            auto priceToIndex(Price price) const noexcept {
                return (price % ME_MAX_PRICE_LEVELS);
            }

            auto getOrdersAtPrice(Price price) const noexcept {
                return price_orders_at_price.at(priceToIndex(price));
            }

            auto addOrdersAtPrice(MarketOrdersAtPrice * new_orders_at_price) noexcept {
                // first, we need to add it to our hashmap
                price_orders_at_price.at(priceToIndex(new_orders_at_price->price)) = new_orders_at_price;

                // now, we need to find the right place to put it, starting at the best BUY/SELL
                const auto best_orders_by_price = (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price);

                // if it doesn't exist, we need to set this as the first price level that exists
                if (UNLIKELY(!best_orders_by_price)) {
                    (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price) = new_orders_at_price;
                    new_orders_at_price->prev_entry = new_orders_at_price->next_entry = new_orders_at_price;
                
                } else { // find the right place to put it in the list and add it

                    auto target = best_orders_by_price;
                    bool add_after = (
                                        (new_orders_at_price->side == Side::SELL && new_orders_at_price->price > target->price) 
                                        ||
                                        (new_orders_at_price->side == Side::BUY && new_orders_at_price->price < target->price)
                                        );

                    // check if it needs to go 2 or more past the current one
                    if (add_after) {
                        target = target->next_entry;
                        add_after = (
                                        (new_orders_at_price->side == Side::SELL && new_orders_at_price->price > target->price) 
                                        ||
                                        (new_orders_at_price->side == Side::BUY && new_orders_at_price->price < target->price)
                                    );
                    }

                    // go until we find the one such that it doesn't need to go after it
                    while(add_after && target != best_orders_by_price) {
                        add_after = (
                                        (new_orders_at_price->side == Side::SELL && new_orders_at_price->price > target->price) 
                                        ||
                                        (new_orders_at_price->side == Side::BUY && new_orders_at_price->price < target->price)
                                    );
                        if (add_after) {
                            target = target->next_entry;
                        }
                    }

                    if (add_after) { // add after target

                        if (target == best_orders_by_price) {
                            target = best_orders_by_price->prev_entry;
                        }

                        new_orders_at_price->prev_entry = target; // inserting after target
                        target->next_entry->prev_entry = new_orders_at_price; // fixing next's backwards link
                        new_orders_at_price->next_entry = target->next_entry; // inserting before target->next
                        target->next_entry = new_orders_at_price; // fixing target's forward link

                    } else { // add before target

                        // fix new orders at price links
                        new_orders_at_price->prev_entry = target->prev_entry;
                        new_orders_at_price->next_entry = target;

                        // fix the links of target and its prev
                        target->prev_entry->next_entry = new_orders_at_price;
                        target->prev_entry = new_orders_at_price;

                        // now, we need to fix the doubly linked part of the best_orders_at_price
                        if (
                            (new_orders_at_price->side == Side::BUY && new_orders_at_price->price > best_orders_by_price->price) 
                            ||
                            (new_orders_at_price->side == Side::SELL && new_orders_at_price->price < best_orders_by_price->price)
                            ) {
                                // fix the 'next' entry of target
                                target->next_entry = (target->next_entry == best_orders_by_price ? new_orders_at_price : target->next_entry);

                                // fix the pointer of the best for BUY and best for SELL
                                (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price) = new_orders_at_price;
                        }

                    }


                }


            }

    };

    typedef std::array<MarketOrderBook *, ME_MAX_TICKERS> MarketOrderBookHashMap;

}