#pragma once

#include "utils/macros.h"
#include "utils/orderinfo_types.h"
#include "utils/logger.h"

#include "exchange/order_gateway/client_response.h"

#include "market_order_book.h"

using namespace Common;

namespace Trading {

    // struct helps us organize our positions for each asset
    // NOTE: vwap = volume weighted average price
    struct PositionInfo {
        int32_t position = 0;
        double real_pnl = 0, unreal_pnl = 0, total_pnl = 0;
        std::array<double, sideToIndex(Side::MAX) + 1> open_vwap; // product of price and execution qty on each side
        Qty volume = 0;
        const BBO *bbo = nullptr;

        std::string toString() const {
            std::stringstream ss;
            ss << "Position{"
                << " pos: " << position
                << " r-pnl: " << real_pnl
                << " u-pnl: " << unreal_pnl
                << " t-pnl: " << total_pnl
                << " vol: " << volume
                << " vwaps:[ " << (position ? open_vwap.at(sideToIndex(Side::BUY)) / std::abs(position) : 0)
                << " X " << (position ? open_vwap.at(sideToIndex(Side::SELL) / std::abs(position)) : 0)
                << " ] "
                << (bbo ? bbo->toString() : "") << "}";

            return ss.str();
        }


        /*
            This is called when we get the FILLED response
            --
            position will keep track of the the positive or negative qty
            open vwap will be the total amount of 'money' we have spent
            vwap will be the average price basis of our qty
            realized pnl is updated whenever a trade occurs opposite of our qty
            unrealized pnl is updated based on the price from the most recent market update and curr VWAP
        */
        void addFill(const Exchange::MEClientResponse *client_response, Logger *logger) noexcept {

            // extract some vars
            const auto old_position = position;
            const auto side_index = sideToIndex(client_response->side);
            const auto opp_side_index = sideToIndex(client_response->side == Side::BUY ? Side::SELL : Side::BUY);
            const auto side_value = sideToValue(client_response->side);

            // first, we update the position based on what was executed in the exchange
            position += client_response->exec_qty * side_value;
            volume += client_response->exec_qty;

            // next, we need to update the open_vwap
            if (old_position * sideToValue(client_response->side) >= 0) {
                // notice, we don't need to update any other variables if we were 'flat' before or add to the same side
                // in other words, there were no crosses
                open_vwap[side_index] += (client_response->price * client_response->exec_qty);

            } else {
                // otherwise, we need to update the realized pnl because the execution has gone against our position

                // avg price basis of our previous position 
                const auto opp_side_vwap = open_vwap[opp_side_index] / std::abs(old_position);

                // new position's 'total money spent'
                open_vwap[opp_side_index] = opp_side_vwap * std::abs(position);

                // critical pnl calculation, note that we might 'flip sides' which is why we need the min, the remaining amount will spill over into our current position
                real_pnl += std::min(static_cast<int32_t>(client_response->exec_qty), std::abs(old_position)) * (opp_side_vwap - client_response->price) * sideToValue(client_response->side);

                // handling 'flip' case
                if (position * old_position < 0) {
                    open_vwap[side_index] = (client_response->price * std::abs(position));
                    open_vwap[opp_side_index] = 0;
                }
            }

            // lastly, we update the unrealized pnl
            if(!position) {
                // if we don't have a position, we don't have any unaccounted/future pnl
                open_vwap[sideToIndex(Side::BUY)] = open_vwap[sideToIndex(Side::SELL)] = 0;
                unreal_pnl = 0;

            } else {

                if (position > 0) {
                    unreal_pnl = (client_response->price - open_vwap[sideToIndex(Side::BUY)] / std::abs(position)) * std::abs(position);
                } else {
                    unreal_pnl = (open_vwap[sideToIndex(Side::SELL)] / std::abs(position) - client_response->price) * std::abs(position);
                }
            }

            // update total forecasted pnl
            total_pnl = real_pnl + unreal_pnl;
 
            // log the change made to our position
            std::string time_str;
            logger->log("%:% %() % % %\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                toString(), client_response->toString().c_str()
            );

        } 

        /*
            This is called when we get an ADDED response, indicating changes in the order book
        */
        void updateBBO(const BBO *bbo_ptr, Logger *logger) noexcept {
            std::string time_str;
            bbo = bbo_ptr; // the bbo_ptr is coming from the order book for this asset

            // if we have a flat position or the bbo indicates that there are no uncrossed orders
            // then, we do nothing
            if (position && bbo->bid_price != Price_INVALID && bbo->ask_price != Price_INVALID) {
                
                // we'll use the mid price to estimate the unrealized pnl
                const auto mid_price = (bbo->bid_price + bbo->ask_price) * 0.5;

                if (position > 0) {
                    unreal_pnl = (mid_price - open_vwap[sideToIndex(Side::BUY)] / std::abs(position)) * std::abs(position);
                } else {
                    unreal_pnl = (open_vwap[sideToIndex(Side::SELL)] / std::abs(position) - mid_price) * std::abs(position);
                }

                const auto old_total_pnl = total_pnl;
                total_pnl = real_pnl + unreal_pnl;

                if (total_pnl != old_total_pnl) {
                    logger->log("%:% %() % % %\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                        toString(), bbo->toString()
                    );
                }
            }
        }
    };


    class PositionKeeper {

        private:
            std::string time_str;
            Common::Logger *logger = nullptr;

            std::array<PositionInfo, ME_MAX_TICKERS> ticker_position;

        public:
            const PositionInfo * getPositionInfo(TickerId ticker_id) const noexcept {
                return &(ticker_position.at(ticker_id));
            }

    };

}