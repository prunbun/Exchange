#pragma once

#include "../../utils/thread_utils.h"
#include "../../utils/lock_free_queue.h"
#include "../../utils/macros.h"
#include "../../utils/logger.h"

#include "../order_gateway/client_request.h"
#include "../order_gateway/client_response.h"
#include "../market_publisher/market_update.h"

namespace Exchange {

    class MatchingEngine final {
        public:
            MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, MEMarketUpdateLFQueue *market_updates);
            
            ~MatchingEngine();

            MatchingEngine() = delete;
            MatchingEngine(const MatchingEngine &) = delete;
            MatchingEngine(const MatchingEngine &&) = delete;
            MatchingEngine &operator=(const MatchingEngine &) = delete;
            MatchingEngine &operator=(const MatchingEngine &&) = delete;

            // starts an infinite loop that is listening for client requests from the gateway LFQ
            void start() {
                running = true;

                ASSERT(createAndStartThread(-1, "Exchange/MatchingEngine", [this]() {run();}) != nullptr, "failed to start Matching Engine thread");
            }

            // stops the infinite loop that is listening for client requests from the gateway LFQ
            void stop() {
                running = false;
            }

            // accepts incoming requests from the order gateway and sends them for processing
            void run() noexcept {
                logger.log("%:% %() % \n",
                    __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_string)
                );

                while(running) {
                    const MEClientRequest * me_client_request = incoming_requests->getNextRead();
                    if (LIKELY(me_client_request)) {
                        
                        logger.log("%:% %() % Processing % \n",
                            __FILE__, __LINE__, __FUNCTION__,
                            Common::getCurrentTimeStr(&time_string), me_client_request->toString()
                        );

                        processClientRequest(me_client_request);
                        incoming_requests->updateReadIndex();
                    }
                }
            }

            // find out which ticker it is for and forward the request to that order book
            void processClientRequest(const MEClientRequest * client_request) noexcept {
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

            // once the order book is done producing a response, the engine sends the response to the gateway
            void sendClientResponse(const MEClientResponse * client_response) noexcept {
                logger.log("%:% %() % Sending % \n",
                    __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_string), client_response->toString()
                );

                // notice how we have a pointer to an object instead of the obj itself
                // therefore, we use a move instead of assigning the object directly
                MEClientResponse * next_write = outgoing_responses->getNextWriteTo();
                *next_write = std::move(*client_response);
                outgoing_responses->updateWriteIndex();

                // note that now, client_response is pointing to free memory!
                // it will go out of scope, but this is good practice
                // this is not necessary, but for learning purposes
                client_response = nullptr; 
            }

            void sendMarketUpdate(const MEMarketUpdate * market_update) {
                logger.log("%:% %() % Sending % \n",
                    __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_string), market_update->toString()
                );

                // notice how we have a pointer to an object instead of the obj itself
                // therefore, we use a move instead of assigning the object directly
                MEMarketUpdate * next_write = outgoing_market_updates->getNextWriteTo();
                *next_write = std::move(*market_update);
                outgoing_market_updates->updateWriteIndex();

                // this is not necessary, but for learning purposes
                market_update = nullptr; 
            }


        private:
            OrderBookHashMap ticker_order_book;
            ClientRequestLFQueue *incoming_requests = nullptr;
            ClientResponseLFQueue *outgoing_responses = nullptr;
            MEMarketUpdateLFQueue *outgoing_market_updates = nullptr;

            volatile bool running = false;

            std::string time_string;
            Logger logger;

    };

}