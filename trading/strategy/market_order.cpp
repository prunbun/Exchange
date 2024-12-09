#include "market_order.h"

std::string Trading::MarketOrder::toString() const {
    
    std::stringstream ss;

    ss << "MarketOrder" << "["
    << "order_id: " << orderIdToString(order_id) << " "
    << "side: " << sideToString(side) << " "
    << "price " << priceToString(price) << " "
    << "qty: " << qtyToString(qty) << " "
    << "priority: " << priorityToString(priority) << " "
    << "prev_order: " << orderIdToString(prev_order ? prev_order->order_id : OrderId_INVALID) << " "
    << "next_order: " << orderIdToString(next_order ? next_order->order_id : OrderId_INVALID) << "]";

    return ss.str(); 
}

std::string Trading::MarketOrdersAtPrice::toString() const {

    std::stringstream ss;

    ss << "MarketOrdersAtPrice" << "["
    << "Side: " << sideToString(side) << " "
    << "Price: " << priceToString(price) << " "
    << "First_Market_Order" << orderIdToString(first_mkt_order ? first_mkt_order->order_id : OrderId_INVALID) << " "
    << "Prev_Entry: " << priceToString(prev_entry ? prev_entry->price : Price_INVALID) << " "
    << "Next_Entry: " << priceToString(next_entry ? next_entry->price : Price_INVALID) << "]";

    return ss.str();
}
