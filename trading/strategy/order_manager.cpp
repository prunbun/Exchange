#include "order_manager.h"

// sends a new order to the client order gateway
void Trading::OrderManager::newOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept
{
    // first, we place the order with the specified details
    const Exchange::MEClientRequest new_request{Exchange::ClientRequestType::NEW, trade_engine->client_id(), ticker_id, next_order_id, side, price, qty};
    trade_engine->sendClientRequest(&new_request);

    // next it updates the order state in the OMOrder struct
    *order = {ticker_id, next_order_id, side, price, qty, OMOrderState::PENDING_NEW};
    ++next_order_id;

    logger->log("%:% %() % Sent new order % for %\n"
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        new_request.toString().c_str(), order->toString().c_str()
    );

}

// sends a cancel order to the client order gateway
void Trading::OrderManager::cancelOrder(OMOrder *order) noexcept
{

    // first, we place the cancel request
    const Exchange::MEClientRequest cancel_request{Exchange::ClientRequestType::CANCEL, trade_engine->clientId(), order->ticker_id, order->order_id, order->side, order->price, order->qty};
    trade_engine->sendClientRequest(&cancel_request);

    // next, we update the order state in the OMOrder struct
    order->order_state = OMOrderState::PENDING_CANCEL;

    logger->log("%:% %() % Sent cancel % for %\n",
        __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str),
        cancel_request.toString().c_str(), order->toString().c_str()
    );

}

// manages a single order and sends new/cancel requests based on the order state
// for 'INVALID'/'DEAD' orders, checks pre-trade risks and places the order
void Trading::OrderManager::moveOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept
{

    switch (order->order_state) {

        case OMOrderState::LIVE: { // verifies information
            if (order->price != price || order->qty != qty) {
                cancelOrder(order);
            }
        } 
            break;

        case OMOrderState::INVALID:
        case OMOrderState::DEAD: {
            if (LIKELY(price != Price_INVALID)) {
                const RiskCheckResult risk_result = risk_manager.checkPreTradeRisk(ticker_id, side, qty);
                
                if (LIKELY(risk_result == RiskCheckResult::ALLOWED)) {
                    newOrder(order, ticker_id, price, side, qty);
                } else {
                    logger->log("%:% %() % Ticker:% Side:% Qty:% RiskCheckResult:% \n",
                        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                        tickerIdToString(ticker_id), sideToString(side), qtyToString(qty), riskCheckResultToString(risk_result)
                    );
                }
            }
        }
            break;   

        // in these cases, we just have to wait for the order to execute in the exchange
        case OMOrderState::PENDING_NEW:
        case OMOrderState::PENDING_CANCEL:
            break;     
    }

}
