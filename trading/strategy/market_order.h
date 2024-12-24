#pragma once

#include <array>
#include <sstream>
#include "../../utils/orderinfo_types.h"
#include "../../utils/exchange_limits.h"

using namespace Common;

namespace Trading {

    // struct we will store in the client order book for each order
    struct MarketOrder {
        OrderId order_id = OrderId_INVALID;
        Side side = Side::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;

        MarketOrder *prev_order = nullptr;
        MarketOrder *next_order = nullptr;


        MarketOrder() = default;

        MarketOrder(OrderId order_id_param, Side side_param, Price price_param, Qty qty_param, Priority priority_param, 
                    MarketOrder *prev_order_param, MarketOrder *next_order_param
                    ) noexcept :
                    order_id(order_id_param), side(side_param), price(price_param), qty(qty_param), priority(priority_param),
                    prev_order(prev_order_param), next_order(next_order_param) {

        }

        std::string toString() const;
    };

    // way to find this order based on the order id in the order book
    typedef std::array<MarketOrder *, ME_MAX_ORDER_IDs> OrderHashMap;

    // defines a price level in the order book
    struct MarketOrdersAtPrice {
        Side side = Side::INVALID;
        Price price = Price_INVALID;

        MarketOrder *first_mkt_order = nullptr;

        MarketOrdersAtPrice *prev_entry = nullptr;
        MarketOrdersAtPrice *next_entry = nullptr;

        MarketOrdersAtPrice() = default;

        MarketOrdersAtPrice(Side side_param, Price price_param, 
                            MarketOrdersAtPrice *prev_entry_param, MarketOrdersAtPrice *next_entry_param
                            ): side(side_param), price(price_param), prev_entry(prev_entry_param), next_entry(next_entry_param) {

        }

        std::string toString() const;
    };

    typedef std::array<MarketOrdersAtPrice *, Common::ME_MAX_PRICE_LEVELS> OrdersAtPriceHashmap;

    // BBO structure - total quantity availble at the best bid and ask prices - used in QR strategies
    struct BBO {
        Price bid_price = Price_INVALID, ask_price = Price_INVALID;
        Qty bid_qty = Qty_INVALID, ask_qty = Qty_INVALID;

        std::string toString() const {

            std::stringstream ss;

            ss << "BBO{ " 
            << qtyToString(bid_qty) << "@" << priceToString(bid_price) << " X "
            << priceToString(ask_price) << "@" << qtyToString(ask_qty)
            << " }";

            return ss.str();
        }
    };

}