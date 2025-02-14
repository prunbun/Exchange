#include "me_order_book.h"
#include "matching_engine.h"

namespace Exchange {
    MEOrderBook::MEOrderBook(Logger *logger_param, MatchingEngine *matching_engine_param
    ): matching_engine(matching_engine_param), orders_at_price_pool(ME_MAX_PRICE_LEVELS),
    order_pool(ME_MAX_ORDER_IDs), logger(logger_param) {

    }

    // MEOrderBook::MEOrderBook(TickerId ticker_id_param, Logger *logger_param, MatchingEngine *matching_engine_param
    // ): ticker_id(ticker_id_param), matching_engine(matching_engine_param), orders_at_price_pool(ME_MAX_PRICE_LEVELS),
    // order_pool(ME_MAX_ORDER_IDs), logger(logger_param) {

    // }

    MEOrderBook::~MEOrderBook() {
        logger->log("%:% %() % OrderBook destructor \n % \n",
            __FILE__, __LINE__, __FUNCTION__, 
            Common::getCurrentTimeStr(&time_str),
            toString(false, true)
        );

        matching_engine = nullptr;
        bids_by_price = asks_by_price = nullptr;
        for (auto &itr : cid_oid_to_order) {
            itr.fill(nullptr);
        }
    }

    // tries to execute a trade with the given aggressive order and the passive order (iterator)
    void MEOrderBook::match(TickerId instrument_id, ClientId client_id, Side side, OrderId client_order_id,
                OrderId unique_market_order_id, MEOrder * iterator, Qty *leaves_qty) noexcept {
        MEOrder * order = iterator;
        Qty order_qty = order->qty;

        // PART 1: lets execute the trade for as many qty as possible
        Qty fill_qty = std::min(*leaves_qty, order_qty);
        *leaves_qty -= fill_qty;
        order->qty -= fill_qty;

        // now we can notify both the aggressive and passive order owners that a trade occurred
        // note that we execute at the price of the passive trade in the order book

        // aggressive order owner message
        client_response = {ClientResponseType::FILLED, client_id, instrument_id, client_order_id,
                            unique_market_order_id, side, iterator->price, Qty_INVALID, fill_qty, *leaves_qty
                            };
        matching_engine->sendClientResponse(&client_response);

        // passive order owner message
        client_response = {ClientResponseType::FILLED, order->client_id, instrument_id,
                            order->client_order_id, order->market_order_id, order->side,
                            iterator->price, Qty_INVALID, fill_qty, order->qty
                            };
        matching_engine->sendClientResponse(&client_response);

        // market update of the trade, making sure client id's are not revealed
        market_update = {MarketUpdateType::TRADE, OrderId_INVALID, instrument_id, side,
                        iterator->price, fill_qty, Priority_INVALID
                        };
        matching_engine->sendMarketUpdate(&market_update);

        // PART 2: Let's notify the market/participants about changes to the order book
        if (!order->qty) {
            // if we completely filled the passive order, we can remove it from our order book
            market_update = {MarketUpdateType::CANCEL, order->market_order_id, instrument_id,
                            order->side, order->price, order->qty, Priority_INVALID
                            };
            matching_engine->sendMarketUpdate(&market_update);
            removeOrder(order);
        } else {
            // we just tell the market about the modified amount
            market_update = {MarketUpdateType::MODIFY, order->market_order_id, instrument_id,
                            order->side, order->price, order->qty, order->priority
                            };
            matching_engine->sendMarketUpdate(&market_update);
        }
    }

    Qty MEOrderBook::checkForMatch(ClientId client_id, OrderId client_order_id, TickerId instrument_id, 
                        Side side, Price price, Qty qty, OrderId unique_market_order_id) noexcept {
        Qty leaves_qty = qty;

        if (side == Side::BUY) {
            // we want to match on all available sell orders
            
            // keep matching until we have active sell orders and we have some qty left
            while (leaves_qty && asks_by_price) {
                // best sell price we can get
                MEOrder * ask_iterator = asks_by_price->first_me_order;
                if (LIKELY(price < ask_iterator->price)) {
                    // if buyer wants lower than what we can offer, we stop the matching
                    break;
                }

                // otherwise match as much as you can with the current best offer
                match(instrument_id, client_id, side, client_order_id,
                    unique_market_order_id, ask_iterator, &leaves_qty
                    );
            }
        }

        if (side == Side::SELL) {
            while (leaves_qty && bids_by_price) {
                MEOrder * bid_iterator = bids_by_price->first_me_order;
                if (LIKELY(price > bid_iterator->price)) {
                    // if the sell price is higher than any of our buyers, stop the matching
                    break;
                }

                match(instrument_id, client_id, side, client_order_id,
                    unique_market_order_id, bid_iterator, &leaves_qty
                    );
            }
        }

        return leaves_qty;  
    }

    // Handle client order requests that want to enter new orders in the market
    void MEOrderBook::add(ClientId client_id, OrderId client_order_id, TickerId instrument_id, Side side, Price price, Qty qty) noexcept {
        
        // 1. we have to accept the offer and send a receipt to the client that we got it
        OrderId const unique_market_order_id = generateNewMarketOrderId();
        client_response = {ClientResponseType::ACCEPTED, client_id, instrument_id, client_order_id,
                            unique_market_order_id, side, price, qty, 0, qty
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

    // cancels an active order in the order book, if applicable
    void MEOrderBook::cancel(ClientId client_id, OrderId order_id, TickerId instrument_id) noexcept {
        // check if the client id is valid
        bool is_cancelable = (client_id < cid_oid_to_order.size());
        MEOrder *exchange_order = nullptr;
        if (LIKELY(is_cancelable)) {
            // check if the order id is valid and exists
            OrderHashMap &co_itr = cid_oid_to_order.at(client_id);
            exchange_order = co_itr.at(order_id);
            is_cancelable = (exchange_order != nullptr);
        }

        // if either were invalid, we have to tell the matching engine that something went wrong
        if (UNLIKELY(!is_cancelable)) {
            client_response = {ClientResponseType::CANCEL_REJECTED, client_id, instrument_id,
                                order_id, OrderId_INVALID, Side::INVALID, Price_INVALID, Qty_INVALID, Qty_INVALID
                                };
        } else {
            // otherwise, let's cancel it
            client_response = {ClientResponseType::CANCELED, client_id, instrument_id,
                                order_id, exchange_order->market_order_id, exchange_order->side, exchange_order->price, Qty_INVALID, exchange_order->qty
                                };
            market_update = { MarketUpdateType::CANCEL, exchange_order->market_order_id, instrument_id, exchange_order->side,
                                exchange_order->price, 0, exchange_order->priority
                            };
            removeOrder(exchange_order);
            matching_engine->sendMarketUpdate(&market_update);
        }
        
        matching_engine->sendClientResponse(&client_response);
    }
}