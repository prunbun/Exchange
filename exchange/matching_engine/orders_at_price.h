#pragma once

#include "../../utils/orderinfo_types.h"
#include "matching_engine_order.h"

using namespace Common;

namespace Exchange {

    // this file defines a struct that will hold all the orders at a given side
    // it is a node in a doubly linked to find the previous and next price structs
    struct MEOrdersAtPrice {
        Side side = Side::INVALID;
        Price price = Price_INVALID;

        MEOrder *first_me_order = nullptr;

        MEOrdersAtPrice *prev_entry = nullptr;
        MEOrdersAtPrice *next_entry = nullptr;

        MEOrdersAtPrice() = default;

        MEOrdersAtPrice(Side side_param, Price price_param, MEOrder *first_me_order_param, MEOrdersAtPrice *prev_entry_param, MEOrdersAtPrice *next_entry_param)
                        : side(side_param), price(price_param), first_me_order(first_me_order), prev_entry(prev_entry_param), next_entry(next_entry_param) {}

        std::string to_string() const {
            std::stringstream ss;

            ss << "MEordersAtPrice["
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "first_me_order:" << (first_me_order ? first_me_order->toString() : "null") << " "
            << "prev:" << priceToString(prev_entry ? prev_entry->price : Price_INVALID) << " " 
            << "next:" << priceToString(next_entry ? next_entry->price : Price_INVALID) << "]";
            return ss.str();
        }

    };

    // mapping price to MEOrdersAtPrice objects
    typedef std::array<MEOrdersAtPrice *, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap;

}