#pragma once

#include "../utils/thread_utils.h"
#include "../utils/lock_free_queue.h"
#include "../utils/macros.h"
#include "../utils/logger.h"

#include "client_request.h"
#include "client_response.h"
#include "market_update.h"

// #include "matching_engine_order_book.h"

namespace Exchange {

    class MatchingEngine final {
        public:
            MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, MEMarketUpdateLFQueue *market_updates
            ): incoming_requests(client_requests), outgoing_responses(client_responses), outgoing_market_updates(market_updates), logger("exchange_matching_engine.log") {

                for(size_t i = 0; i < ticker_order_book.size(); ++i) {
                    ticker_order_book[i] = new MEOrderBook(i, &logger, this);
                }

            };
            
            ~MatchingEngine() {
                run = false;

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

            MatchingEngine() = delete;
            MatchingEngine(const MatchingEngine &) = delete;
            MatchingEngine(const MatchingEngine &&) = delete;
            MatchingEngine &operator=(const MatchingEngine &) = delete;
            MatchingEngine &operator=(const MatchingEngine &&) = delete;


            void start();

            void stop();


        private:
            OrderBookHashMap ticker_order_book;
            ClientRequestLFQueue *incoming_requests = nullptr;
            ClientResponseLFQueue *outgoing_responses = nullptr;
            MEMarketUpdateLFQueue *outgoing_market_updates = nullptr;

            volatile bool run = false;

            std::string time_string;
            Logger logger;

    };

}