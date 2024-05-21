#pragma once

#include "../../utils/orderinfo_types.h"
#include "../../utils/memory_pool.h"
#include "../../utils/logger.h"
#include "../order_gateway/client_response.h"
#include "../market_publisher/market_update.h"
#include "matching_engine_order.h"
#include "matching_engine.h"
#include "orders_at_price.h"

using namespace Common;

namespace Exchange {

    // each MEOrderBook object holds orders for a single security, in this case, a ticker
    class MEOrderBook final {
        private:
            TickerId ticker_id = TickerId_INVALID; // instrument this order book is for
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
            MEOrderBook(TickerId ticker_id_param, Logger *logger_param, MatchingEngine *matching_engine_param
            ): ticker_id(ticker_id_param), matching_engine(matching_engine_param), orders_at_price_pool(ME_MAX_PRICE_LEVELS),
            order_pool(ME_MAX_ORDER_IDs), logger(logger_param) {

            }

            ~MEOrderBook() {
                logger->log("%:% %() % OrderBook destructor \n % \n",
                    __FILE__, __LINE__, __FUNCTION__, 
                    Common::getCurrentTimeStr(&time_string),
                    toString(false, true)
                );

                matching_engine = nullptr;
                bids_by_price = asks_by_price = nullptr;
                for (auto &itr : cid_oid_to_order) {
                    itr.fill(nullptr);
                }
            }

            MEOrderBook() = delete;
            MEOrderBook(const MEOrderBook &) = delete;
            MEOrderBook(const MEOrderBook &&) = delete;
            MEOrderBook &operator=(const MEOrderBook &) = delete;
            MEOrderBook &operator=(const MEOrderBook &&) = delete;

            // Handle client order requests that want to enter new orders in the market
            void add(ClientId client_id, OrderId client_order_id, TickerId instrument_id, Side side, Price price, Qty qty) {
                
                // 1. we have to accept the offer and send a receipt to the client that we got it
                OrderId const unique_market_order_id = generateNewMarketOrderId();
                client_response = {ClientResponseType::ACCEPTED, client_id, instrument_id, client_order_id,
                                    unique_market_order_id, side, price, 0, qty
                                    };
                matching_engine->sendClientResponse(&client_response); // notice how we use std::move in m.e., so it is ok to reuse this var

                // 2. we need to find out if it can be executed, only if there is some qty left over do we add it to the order book
                const auto leaves_qty = checkForMatch(client_id, client_order_id, instrument_id, side, price, qty, unique_market_order_id);
                if (LIKELY(leaves_qty)) {
                    const Priority priority = getNextPriority(price);
                    MEOrder * order = order_pool.allocate(instrument_id, client_id, client_order_id, unique_market_order_id, side, price,
                                                        leaves_qty, priority, nullptr, nullptr    
                                                        ); // note we are indirectly invoking the MEOrder constructor
                    addOrder(order);
                    
                    // now that a new order has entered the order book, we need to notify the market about it
                    market_update = {MarketUpdateType::ADD, unique_market_order_id, instrument_id, side,
                                    price, leaves_qty, priority
                                    };
                    matching_engine->sendMarketUpdate(&market_update);
                }
            }

            // if a price level already exists, ret priority value +1 higher than last order, else ret 1
            Priority getNextPriority(Price price) noexcept {
                const MEOrdersAtPrice * orders_at_price = getOrdersAtPrice(price);
                if (!orders_at_price) {
                    return 1lu;
                }

                return orders_at_price->first_me_order->prev_order->priority + 1; // note the wrap around
            }

            std::string toString(bool, bool) const {
                return "";
            }

    };

    // maps tickers to their order book object
    typedef std::array<MEOrderBook *, ME_MAX_TICKERS> OrderBookHashMap;
}