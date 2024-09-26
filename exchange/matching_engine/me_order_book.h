#pragma once

#include "../../utils/orderinfo_types.h"
#include "../../utils/memory_pool.h"
#include "../../utils/logger.h"
#include "../order_gateway/client_response.h"
#include "../market_publisher/market_update.h"
#include "matching_engine_order.h"
// #include "matching_engine.h"
#include "orders_at_price.h"

using namespace Common;

namespace Exchange {

    class MatchingEngine;

    // each MEOrderBook object holds orders for a single security, in this case, a ticker
    class MEOrderBook final {
        private:
            // TickerId ticker_id = TickerId_INVALID; // instrument this order book is for
            MatchingEngine *matching_engine = nullptr; // pointer to parent matching engine

            ClientsOrderHashMap cid_oid_to_order; // object to get all orders for a client

            MemPool<MEOrdersAtPrice> orders_at_price_pool;
            MEOrdersAtPrice *bids_by_price = nullptr; // all the bids at this price, can move to other prices
            MEOrdersAtPrice *asks_by_price = nullptr; // all the asks at this price, can move to other prices

            OrdersAtPriceHashMap price_orders_at_price; // maps price to orders at price objects

            MemPool<MEOrder> order_pool; // all the orders we have so far

            // objects to store results of computations
            MEClientResponse client_response;
            MEMarketUpdate market_update;

            // provides a new unqiue id to an order in this order book
            OrderId next_market_order_id = 1;

            std::string time_string;
            Logger *logger = nullptr;

            // returns the current order_id as a unique id, then increments the internal counter
            OrderId generateNewMarketOrderId() noexcept {
                return next_market_order_id++;
            }

            // generates an index from a given price so we can access our price <> MEOrdersAtPrice hashmap
            auto priceToIndex(Price price) const noexcept {
                return (price % ME_MAX_PRICE_LEVELS);
            }

            // gets all the orders at a certain price in this order book
            MEOrdersAtPrice * getOrdersAtPrice(Price price) const noexcept {
                return price_orders_at_price.at(priceToIndex(price));
            }

        public:
            MEOrderBook(Logger *logger_param, MatchingEngine *matching_engine_param);
            // MEOrderBook(TickerId ticker_id_param, Logger *logger_param, MatchingEngine *matching_engine_param);

            ~MEOrderBook();

            MEOrderBook() = delete;
            MEOrderBook(const MEOrderBook &) = delete;
            MEOrderBook(const MEOrderBook &&) = delete;
            MEOrderBook &operator=(const MEOrderBook &) = delete;
            MEOrderBook &operator=(const MEOrderBook &&) = delete;

            // Handle client order requests that want to enter new orders in the market
            void add(ClientId client_id, OrderId client_order_id, TickerId instrument_id, Side side, Price price, Qty qty) noexcept;

            // if a price level already exists, ret priority value +1 higher than last order, else ret 1
            Priority getNextPriority(Price price) noexcept {
                const MEOrdersAtPrice * orders_at_price = getOrdersAtPrice(price);
                if (!orders_at_price) {
                    return 1lu;
                }

                return orders_at_price->first_me_order->prev_order->priority + 1; // note the wrap around
            }

            // adds an order to the order book, NOTE orders can only be on one side at a price, otherwise they are matched!
            void addOrder(MEOrder *order) noexcept {
                
                // let's check if there are any orders at this price already
                const MEOrdersAtPrice * orders_at_price = getOrdersAtPrice(order->price);
                
                // if it doesn't exist, we have to make the MEOrdersAtPrice and insert THAT into the order book as well, else just append to the ll
                if (!orders_at_price) {
                    order->next_order = order->prev_order = order;
                    MEOrdersAtPrice * new_orders_at_price = orders_at_price_pool.allocate(order->side, order->price, order, nullptr, nullptr);
                    addOrdersAtPrice(new_orders_at_price);
                } else {
                    MEOrder * first_order = (orders_at_price ? orders_at_price->first_me_order : nullptr); // note that this should NEVER occur
                    
                    // some linked-list operations now, we want to insert it at the end and want to make the list cyclical
                    first_order->prev_order->next_order = order; // get the LAST order by going backwards and insert this order at the end
                    
                    // insert the links for the current order
                    order->prev_order = first_order->prev_order;
                    order->next_order = first_order;

                    // make it cyclical
                    first_order->prev_order = order;
                }

                // finally, we register this order to the client involved
                cid_oid_to_order.at(order->client_id).at(order->client_order_id) = order;
            }

            /* adds the new orders at price to our big hashmap, 
               also, we add it to the sorted order in our linked list for the corresponding side */
            void addOrdersAtPrice(MEOrdersAtPrice * new_orders_at_price) noexcept {

                // add it to our map of price <> MEOrdersAtPrice
                price_orders_at_price.at(priceToIndex(new_orders_at_price->price)) = new_orders_at_price;

                /* now, we check if there are any existing prices in the order book
                   if no, then insert it as a node and link it to itself
                   if yes, find the place to put it and fix the links */
                
                MEOrdersAtPrice * best_orders_by_price = (new_orders_at_price->side == Side::BUY ?
                    bids_by_price : asks_by_price);
                if (UNLIKELY(!best_orders_by_price)) {
                    // assign it to the right list
                    (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price) 
                                                                                    = new_orders_at_price;
                    // add in the links
                    new_orders_at_price->prev_entry = new_orders_at_price->next_entry = new_orders_at_price;
                } else {
                    MEOrdersAtPrice * target = best_orders_by_price;
                    bool add_after = (
                        ((new_orders_at_price->side == Side::SELL) && (new_orders_at_price->price > target->price))
                      || ((new_orders_at_price->side == Side::BUY) && (new_orders_at_price->price < target->price))   
                        );

                    // is the new price the BEST price, or do we have to traverse the list to find the right spot
                    if (add_after) {
                        target = target->next_entry;
                        add_after = (
                        ((new_orders_at_price->side == Side::SELL) && (new_orders_at_price->price > target->price))
                      || ((new_orders_at_price->side == Side::BUY) && (new_orders_at_price->price < target->price))   
                        );
                    }
                    while (add_after && target != best_orders_by_price) {
                        add_after = (
                        ((new_orders_at_price->side == Side::SELL) && (new_orders_at_price->price > target->price))
                      || ((new_orders_at_price->side == Side::BUY) && (new_orders_at_price->price < target->price))   
                        );
                        if (add_after) {
                            target = target->next_entry;
                        }
                    }

                    // now, either we have to insert it after best_orders_by_price, or before the target
                    if (add_after) {
                        // meaning that the only way the while loop terminated is due to target == best_orders_by_price
                        if (target == best_orders_by_price) {
                            target = best_orders_by_price->prev_entry;
                        }
                        // adjust the links
                        new_orders_at_price->prev_entry = target;
                        target->next_entry->prev_entry = new_orders_at_price;
                        new_orders_at_price->next_entry = target->next_entry;
                        target->next_entry = new_orders_at_price;
                    } else { // add before the target
                        new_orders_at_price->prev_entry = target->prev_entry;
                        new_orders_at_price->next_entry = target;
                        target->prev_entry->next_entry = new_orders_at_price;
                        target->prev_entry = new_orders_at_price;

                        // If it is the new best price
                        if ((new_orders_at_price->side == Side::BUY && new_orders_at_price->price > best_orders_by_price->price) ||
                            (new_orders_at_price->side == Side::SELL && new_orders_at_price->price < best_orders_by_price->price)) {
                            target->next_entry = (target->next_entry == best_orders_by_price ? new_orders_at_price : target->next_entry);
                            (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price) = new_orders_at_price;
                        }
                    }
                }
            }

            // cancels an active order in the order book, if applicable
            void cancel(ClientId client_id, OrderId order_id, TickerId instrument_id) noexcept;

            // removes a given order from our order book
            void removeOrder(MEOrder *order) noexcept {
                // need to remove it from the book, client map, and deallocate memory it was using in the pool
                MEOrdersAtPrice *orders_at_price = getOrdersAtPrice(order->price);

                if (order->prev_order == order) {
                    // we know this is the only order at this price, so we can just delete the entire price
                    removeOrdersAtPrice(order->side, order->price);
                } else {
                    // we have to break the links in the linked list
                    MEOrder * order_before = order->prev_order;
                    MEOrder * order_after = order->next_order;
                    order_before->next_order = order_after;
                    order_after->prev_order = order_before;

                    if (orders_at_price->first_me_order == order) {
                        orders_at_price->first_me_order = order_after;
                    }

                    order->prev_order = order->next_order = nullptr;
                }

                cid_oid_to_order.at(order->client_id).at(order->client_order_id) = nullptr;

                order_pool.deallocate(order);
            }

            void removeOrdersAtPrice(Side side_param, Price price_param) {
                MEOrdersAtPrice * best_orders_by_price = (side_param == Side::BUY ? bids_by_price : asks_by_price);
                MEOrdersAtPrice * orders_at_price = getOrdersAtPrice(price_param);

                // if this price<>order map is the only one in the book for this side, then we need to clear this side
                if (UNLIKELY(orders_at_price->next_entry == orders_at_price)) {
                    (side_param == Side::BUY ? bids_by_price : asks_by_price) = nullptr;
                } else {
                    // else remove it from the side's linked list, update the champ pointer, and nullify its state
                    orders_at_price->prev_entry->next_entry = orders_at_price->next_entry;
                    orders_at_price->next_entry->prev_entry = orders_at_price->prev_entry;

                    if (orders_at_price == best_orders_by_price) {
                        (side_param == Side::BUY ? bids_by_price : asks_by_price) = orders_at_price->next_entry;
                    }

                    orders_at_price->prev_entry = orders_at_price->next_entry = nullptr;
                }

                // now we can remove it from the price map and deallocate it
                price_orders_at_price.at(priceToIndex(price_param)) = nullptr;
                orders_at_price_pool.deallocate(orders_at_price);
            }

            Qty checkForMatch(ClientId client_id, OrderId client_order_id, TickerId instrument_id, 
                                Side side, Price price, Qty qty, OrderId unique_market_order_id) noexcept;

            // tries to execute a trade with the given aggressive order and the passive order (iterator)
            void match(TickerId instrument_id, ClientId client_id, Side side, OrderId client_order_id,
                        OrderId unique_market_order_id, MEOrder * iterator, Qty *leaves_qty) noexcept;

            std::string toString(bool, bool) const {
                return "";
            }

    };

    // maps tickers to their order book object
    typedef std::array<MEOrderBook *, ME_MAX_TICKERS> OrderBookHashMap;
}