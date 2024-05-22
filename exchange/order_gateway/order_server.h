#pragma once

#include <functional>

#include "utils/thread_utils.h"
#include "utils/macros.h"
#include "utils/tcp_server.h"

#include "client_request.h"
#include "client_response.h"
#include "utils/exchange_limits.h"
#include "fifo_sequencer.h"

namespace Exchange {

    /* the OrderServer is in charge or receiving client connections, sequence them in FIFO format,
        and exchange information with the matching engine before providing a response to the client
    */ 
    class OrderServer {

        private:
            const std::string interface;
            const int port = 0;

            ClientResponseLFQueue * outgoing_responses = nullptr; // from matching engine
            
            volatile bool running; // marked volatile to stop compiler optimizations like *const prop*

            std::string time_string;
            Logger logger;

            // maps for managing connections
            std::array<size_t, ME_MAX_NUM_CLIENTS> cid_next_outgoing_seq_number; 
            std::array<size_t, ME_MAX_NUM_CLIENTS> cid_next_expected_seq_number; 
            std::array<Common::TCPSocket *, ME_MAX_NUM_CLIENTS> cid_tcp_socket;

            Common::TCPServer tcp_server;

            FIFOSequencer fifo_sequencer;

        public:

            // this function defines what we want the server to do whenever it receives a message
            // remember that in this design, it is really the socket recv() that we set up to listen to the client that executes the callback
            void recvCallback(TCPSocket *socket, Nanos rx_time) noexcept {
                logger.log("%:% %() % Received socket:% len% rx:% \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                    socket->socket_file_descriptor, socket->next_receive_valid_index, rx_time
                );

                // first we have to check if we received enough data for an OMClientRequest
                if (socket->next_receive_valid_index >= sizeof(OMClientRequest)) {

                    // if we have, let's fetch all of them that we have and send them into the queue
                    size_t i = 0;
                    for (; i + sizeof(OMClientRequest) <= socket->next_receive_valid_index; i += sizeof(OMClientRequest)) {
                        const OMClientRequest * request = reinterpret_cast<const OMClientRequest *>(socket->receive_buffer + i);

                        logger.log("%:% %() % Gateway received OMClientRequest:% \n",
                            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                            request->toString()
                        );

                        // PART 1: Check that the request is valid

                        // if this is the first time we are receiving a connection from this client, let's store the socket
                        if (UNLIKELY(cid_tcp_socket[request->me_client_request.client_id] == nullptr)) {
                            cid_tcp_socket[request->me_client_request.client_id] = socket;
                        }

                        // if there was an exisiting socket, but it doesn't match our current socket, throw an error
                        // unique clients must only communicate through their assigned socket from the server
                        if (cid_tcp_socket[request->me_client_request.client_id] != socket) {
                            logger.log("%:% %() % Received ClientRequest from ClientId:% on a different socket:% but expected:% \n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                                request->me_client_request.client_id, socket->socket_file_descriptor,
                                cid_tcp_socket[request->me_client_request.client_id]->socket_file_descriptor
                            );
                            continue;
                        } 

                        // now, lets make sure the sequence number is correct
                        size_t &next_expected_sequence_number = cid_next_expected_seq_number[request->me_client_request.client_id];
                        if (request->seq_number != next_expected_sequence_number) {
                            logger.log("%:% %() % Incorrect sequence number. ClientId:% SeqNum expected:% but received:% \n",
                                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                                request->me_client_request.client_id, next_expected_sequence_number,
                                request->seq_number
                            );
                            continue;
                        }

                        // PART 2: forward the request and time to the FIFO sequencer, so it can be sent to the m.e.
                        // note that we only send the me_client_request, in the type the m.e. expects
                        ++next_expected_sequence_number;
                        fifo_sequencer.addClientRequest(rx_time, request->me_client_request);
                    }

                    // after we have finished processing as many messages as we can, we update the socket's buffer
                    memcpy(socket->receive_buffer, socket->receive_buffer + i, socket->next_receive_valid_index - i);
                    socket->next_receive_valid_index -= i;
                }
            }

            // this is called by the server after it has called the recv callback on all available read sockets
            // we have received all messages in this iter and we can instruct the sequencer to send them to the m.e.
            void recvFinishedCallback() noexcept {
                fifo_sequencer.sequenceAndPublish();
            }


            OrderServer(ClientRequestLFQueue * client_requests, ClientResponseLFQueue * client_responses,
                        const std::string &interface_param, int port_param
                        ): interface(interface_param), port(port_param), outgoing_responses(client_responses),
                        logger("exchange_order_server.log"), tcp_server(logger), fifo_sequencer(client_requests, &logger) {
                
                cid_next_outgoing_seq_number.fill(1);
                cid_next_expected_seq_number.fill(1);
                cid_tcp_socket.fill(nullptr);

                tcp_server.receive_callback = [this] (auto socket, auto rx_time) {
                    recvCallback(socket, rx_time);
                };
                tcp_server.receive_finished_callback = [this] () {
                    recvFinishedCallback();
                };
            };
            
            ~OrderServer() {
                stop();

                // this is normal for thread-based applications to wait for some time to let any pending tasks finish
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(1s);
            };

            void start() {
                running = true;
                tcp_server.listen(interface, port);

                ASSERT(Common::createAndStartThread(-1, "Exchange/OrderServer", [this]() {run();}) != nullptr, 
                        "failed to start OrderServer thread");
            };

            void stop() {
                running = false;
            };

            // runs the server that drives the order gateway
            void run() noexcept {
                logger.log("%:% %() % Order server running... \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string)
                );

                while(running) {
                    // run the server
                    tcp_server.poll();
                    tcp_server.sendAndReceive();

                    // also want to send out the client responses to placed orders
                    for (auto client_response = outgoing_responses->getNextRead(); outgoing_responses->size() && client_response; 
                        client_response = outgoing_responses->getNextRead()) {

                        auto &next_outgoing_seq_number = cid_next_outgoing_seq_number[client_response->client_id];
                        logger.log("%:% %() % Processing cid:% seq:% % \n",
                            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                            client_response->client_id, next_outgoing_seq_number, client_response->toString()
                        );

                        // note that we effectively send OMClientResponse by stacking the sends
                        ASSERT(cid_tcp_socket[client_response->client_id] != nullptr,
                         "Don't have a TCPSocket for ClientId:" + std::to_string(client_response->client_id));
                        cid_tcp_socket[client_response->client_id]->send(&next_outgoing_seq_number, sizeof(next_outgoing_seq_number));
                        cid_tcp_socket[client_response->client_id]->send(client_response, sizeof(MEClientResponse));
                        
                        outgoing_responses->updateReadIndex();

                        ++next_outgoing_seq_number;
                    }
                }
            }

    };

}