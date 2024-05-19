#pragma once

#include <array>
#include <sstream>
#include "orderinfo_types.h"
#include "exchange_limits.h"

using namespace Common;

namespace Exchange {

    // this data type represents an individual order in the matchinge engine
    // we can think of it as a node in a doubly linked list 

    struct MEOrder {
        TickerId ticker_id = TickerId_INVALID;
        ClientId client_id = ClientId_INVALID;
        OrderId client_order_id = OrderId_INVALID;
        OrderId market_order_id = OrderId_INVALID;
        Side side = Side::INVALID;
        Price price = Price_INVALID;
        Qty qty = Qty_INVALID;
        Priority priority = Priority_INVALID;

        MEOrder *prev_order = nullptr;
        MEOrder *next_order = nullptr;

        MEOrder() = default;

        MEOrder(TickerId ticker_id_param, ClientId client_id_param, OrderId client_order_id_param,
                OrderId market_order_id_param, Side side_param, Price price_param, Qty qty_param, Priority priority_param, 
                MEOrder *prev_order_param, MEOrder *next_order_param      
        ) noexcept : 
        ticker_id(ticker_id_param), client_id(client_id_param), client_order_id(client_order_id_param),
        market_order_id(market_order_id_param), side(side_param), price(price_param), qty(qty_param),
        priority(priority_param), prev_order(prev_order_param), next_order(next_order_param) {}

        std::string toString() const {
            std::stringstream ss;
            
            ss << "MEOrder " << "["
            << "ticker:" << tickerIdToString(ticker_id) << " "
            << "cid:" << clientIdToString(client_id) << " "
            << "oid:" << orderIdToString(client_order_id) << " "
            << "moid:" << orderIdToString(market_order_id) << " "
            << "side:" << sideToString(side) << " "
            << "price:" << priceToString(price) << " "
            << "qty:" << qtyToString(qty) << " "
            << "prio:" << priorityToString(priority) << " "
            << "prev:" << orderIdToString(prev_order ? prev_order->market_order_id : OrderId_INVALID) << " "
            << "next:" << orderIdToString(next_order ? next_order->market_order_id : OrderId_INVALID) 
            << "]";

            return ss.str();
        }
    };

    // mapping from OrderId to an MEOrder pointer
    typedef std::array<MEOrder *, ME_MAX_ORDER_IDs> OrderHashMap; 

    // mapping from ClientID to OrderHashMap object
    typedef std::array<OrderHashMap, ME_MAX_NUM_CLIENTS> ClientsOrderHashMap;

}