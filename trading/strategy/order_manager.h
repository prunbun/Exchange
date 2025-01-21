#pragma once

#include "utils/macros.h"
#include "utils/logger.h"

#include "exchange/order_gateway/client_response.h"
#include "exchange/order_gateway/client_request.h"

#include "om_order.h"
#include "risk_manager.h"

using namespace Common;

namespace Trading {

    class TradeEngine;

    class OrderManager {

        private:
            TradeEngine * trade_engine = nullptr;
            const RiskManager& risk_manager;

            std::string time_str;
            Common::Logger *logger = nullptr;

            OMOrderTickerSideHashMap ticker_order_hashmap;
            OrderId next_order_id = 1;
            
            // lower-level private methods for order management
            void newOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept;
            void cancelOrder(OMOrder *order) noexcept;
            void moveOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept;

        public:
            OrderManager(Common::Logger *logger_param, TradeEngine *trade_engine_param, RiskManager& risk_manager_param) 
                : trade_engine(trade_engine_param), risk_manager(risk_manager_param), logger(logger_param)
            {

            }

            OrderManager() = delete;
            OrderManager(const OrderManager &) = delete;
            OrderManager(const OrderManager &&) = delete;
            OrderManager &operator=(const OrderManager &) = delete;
            OrderManager &operator=(const OrderManager &&) = delete;

            const OMOrderSideHashMap * getOMOrderSideHashmap(TickerId ticker_id) const {
                return &(ticker_order_hashmap.at(ticker_id));
            }


            // primary method used by trading strategies to generate and manage orders
            void moveOrders(TickerId ticker_id, Price bid_price, Price ask_price, Qty trade_size) noexcept {

                auto bid_order = &(ticker_order_hashmap.at(ticker_id).at(sideToIndex(Side::BUY)));
                moveOrder(bid_order, ticker_id, bid_price, Side::BUY, trade_size);

                auto sell_order = &(ticker_order_hashmap.at(ticker_id).at(sideToIndex(Side::SELL)));
                moveOrder(sell_order, ticker_id, ask_price, Side::SELL, trade_size);

                return;
            }

            // handles incoming responses from the exchange
            void onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept {

                logger->log("%:% %() % Incoming order update: %\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    client_response->toString().c_str()
                );

                // fetch the corresponding order
                OMOrder * order = &(ticker_order_hashmap.at(client_response->ticker_id).at(sideToIndex(client_response->side)));
                logger->log("%:% %() % Order in-map state: %\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    order->toString().c_str()
                );

                // update the order state
                switch (client_response->type) {

                    // these are cases that are following what we expect
                    case Exchange::ClientResponseType::ACCEPTED: {
                        order->order_state = OMOrderState::LIVE;
                    }
                        break;

                    case Exchange::ClientResponseType::CANCELED: {
                        order->order_state = OMOrderState::DEAD;
                    }
                        break;

                    // if our order gets filled, then we declare the order DEAD if the entire order was filled
                    case Exchange::ClientResponseType::FILLED: {
                        order->qty = client_response->leaves_qty;

                        if (!order->qty) {
                            order->order_state = OMOrderState::DEAD;
                        }
                    }
                        break;

                    // we ignore the others because there is nothing we can do in these cases
                    case Exchange::ClientResponseType::CANCEL_REJECTED:
                    case Exchange::ClientResponseType::INVALID: {
                    }
                        break;
                }
            }
            
    };

}