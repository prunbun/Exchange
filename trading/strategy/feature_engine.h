#pragma once

#include "utils/macros.h"
#include "utils/logger.h"
#include "utils/orderinfo_types.h"
#include "market_order_book.h"

using namespace Common;

namespace Trading {

    constexpr auto Feature_INVALID = std::numeric_limits<double>::quiet_NaN(); // quiet NaN does not cause exception

    /*
        This feature engine computes two features
        1. Market Price - based on top of book prices
        2. Aggressive Trade Qty Ratio - how big a trade is compared to the top of book qtys
    */
    class FeatureEngine {

        private:
            std::string time_str;
            Common::Logger *logger = nullptr;

            // weighted average of what market values this asset
            double mkt_price = Feature_INVALID; 

            // how big an aggressive trade is compared to passive orders
            // indicating if there is more crossing of orders if this ratio is high
            double aggr_trade_qty_ratio = Feature_INVALID; 

        public:
            FeatureEngine(Common::Logger *logger_param);

            // getters for computed features
            double getMktPrice() const noexcept {
                return mkt_price;
            }

            double getAggrTradeQtyRatio() const noexcept {
                return aggr_trade_qty_ratio;
            }

            // called when the client updates its orderbook
            // we can compute the fair market price feature
            void onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook * book) {
                
                // first, let's get the state of this orderbook
                const BBO * bbo = book->getBBO();

                // remember, bbo would be invalid if there were no uncrossed bids or offers
                if(LIKELY(bbo->bid_price != Price_INVALID && bbo->ask_price != Price_INVALID)) {
                    mkt_price = (bbo->bid_price * bbo->ask_qty + bbo->ask_price * bbo->bid_qty) / (static_cast<double>(bbo->bid_qty + bbo->ask_qty));
                }

                logger->log("%:% %() % ticker:% price:% side:% mkt-price:% aggr-trade-ratio:% \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    ticker_id, Common::priceToString(price).c_str(), Common::sideToString(side).c_str(),
                    mkt_price, aggr_trade_qty_ratio
                );

            }

            // called when the client receives a TRADE update on the exchange multicast
            // we can compute the aggressive trade ratio feature
            void onTradeUpdate(const Exchange::MEMarketUpdate * market_update, const MarketOrderBook * book) noexcept {

                // state of orderbook
                const BBO * bbo = book->getBBO();

                // higher ratio means more orders are being crossed
                if(LIKELY(bbo->bid_price != Price_INVALID && bbo->ask_price != Price_INVALID)) {
                    aggr_trade_qty_ratio = static_cast<double>(market_update->qty) / (market_update->side == Side::BUY ? bbo->ask_qty : bbo->bid_qty);
                }

                logger->log("%:% %() % % mkt-price:% aggr-trade_ratio:% \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    market_update->toString().c_str(), mkt_price, aggr_trade_qty_ratio
                );

            }

    };

}