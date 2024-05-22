#include "matching_engine.h"

namespace Exchange {
    MatchingEngine::MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, MEMarketUpdateLFQueue *market_updates
    ): incoming_requests(client_requests), outgoing_responses(client_responses), outgoing_market_updates(market_updates), logger("exchange_matching_engine.log") {

        for(size_t i = 0; i < ticker_order_book.size(); ++i) {
            // ticker_order_book[i] = new MEOrderBook(i, &logger, this);
            ticker_order_book[i] = new MEOrderBook(&logger, this);
        }

    };

    MatchingEngine::~MatchingEngine() {
        running = false;

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);

        // note, we do not free them here since these pointers came from outside the class
        // it is not the responsibility of this class to free that memory since it did not allocate it
        incoming_requests = nullptr;
        outgoing_responses = nullptr;
        outgoing_market_updates = nullptr;

        for (auto &order_book : ticker_order_book) {
            delete order_book;
            order_book = nullptr;
        } 
    }
    
    // find out which ticker it is for and forward the request to that order book
    void MatchingEngine::processClientRequest(const MEClientRequest * client_request) noexcept {
        MEOrderBook * order_book = ticker_order_book[client_request->ticker_id];

        // depending on what type of request it is, we call a different order book function
        switch (client_request->type) {
            case ClientRequestType::NEW: {
                order_book->add(client_request->client_id,
                                client_request->order_id,
                                client_request->ticker_id,
                                client_request->side, client_request->price,
                                client_request->qty
                                );
            }
                break;

            case ClientRequestType::CANCEL: {
                // notice how we don't provide more params than necessary to the func
                order_book->cancel(client_request->client_id, client_request->order_id, client_request->ticker_id);
            }
                break;

            default: {
                FATAL("Received invalid client-request-type: " + clientRequestTypeToString(client_request->type));
            }
                break;
        }
    }
}