#pragma once

#include "utils/thread_utils.h"
#include "utils/macros.h"
#include "utils/logger.h"

#include "order_gateway/client_request.h"

namespace Exchange {
    constexpr size_t ME_MAX_PENDING_REQUESTS = 1024;

    class FIFOSequencer {
        private:
            ClientRequestLFQueue *incoming_requests = nullptr;

            std::string time_str;
            Logger * logger = nullptr;

            // way we will store the time along with the request
            struct RecvTimeClientRequest {
                Nanos recv_time = 0;
                MEClientRequest request;

                // overload an operator that allows us to compare two requests for sorting
                auto operator<(const RecvTimeClientRequest &rhs) const {
                    return (recv_time < rhs.recv_time);
                };
            };

            std::array<RecvTimeClientRequest, ME_MAX_PENDING_REQUESTS> pending_client_requests;
            size_t pending_size = 0;

        public:

            FIFOSequencer(ClientRequestLFQueue *client_requests, Logger * logger_param
            ): incoming_requests(client_requests), logger(logger_param) {} 
            FIFOSequencer() = delete;
            FIFOSequencer(const FIFOSequencer &) = delete;
            FIFOSequencer(const FIFOSequencer &&) = delete;
            FIFOSequencer &operator=(const FIFOSequencer &) = delete;
            FIFOSequencer &operator=(const FIFOSequencer &&) = delete;

            // we just want to add this request the queue, we will sort it before publishing it
            void addClientRequest(Nanos rx_time, const MEClientRequest &request) {
                if (pending_size >= pending_client_requests.size()) {
                    FATAL("Too many pending requests");
                }
                pending_client_requests.at(pending_size++) = RecvTimeClientRequest{rx_time, request};
            }

            // sorts the entries and writes them to the lfq
            void sequenceAndPublish() {
                // if there are no entries that need to be published, return
                if (UNLIKELY(!pending_size)) {
                    return;
                }

                logger->log("%:% %() % Processing % requests \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    pending_size
                );

                // sort the entries
                std::sort(pending_client_requests.begin(), pending_client_requests.begin() + pending_size);

                // write all of them to the LFQ so the matching engine can process them
                for (size_t i = 0; i < pending_size; ++i) {
                    const auto &client_request = pending_client_requests.at(i);
                    logger->log("%:% %() % FIFO sending to M. E. RX:% Req:% \n",
                        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                        client_request.recv_time, client_request.request.toString()
                    );

                    MEClientRequest * next_write = incoming_requests->getNextWriteTo();
                    *next_write = std::move(client_request.request);
                    incoming_requests->updateWriteIndex();

                    // second stage a client request goes through in the exchange
                    TTT_MEASURE(T2_OrderServer_LFQueue_write, (*logger));
                } 

                // empty our sequencer
                pending_size = 0;
            }

    };

}