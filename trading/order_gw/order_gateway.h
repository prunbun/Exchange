#pragma once

#include "utils/thread_utils.h"
#include "utils/macros.h"
#include "utils/tcp_server.h"

#include "exchange/order_gateway/client_request.h"
#include "exchange/order_gateway/client_response.h"

namespace Trading {

    class OrderGateway {

        private:
            const ClientId client_id;

            std::string ip;
            const std::string interface;
            const int port = 0;

            Exchange::ClientRequestLFQueue * outgoing_requests = nullptr;
            Exchange::ClientResponseLFQueue * incoming_responses = nullptr;

            volatile bool running = false;

            std::string time_str;
            Logger logger;

            size_t next_outgoing_seq_number = 1;
            size_t next_expected_sequence_number = 1;
            Common::TCPSocket tcp_socket;

        public:
            OrderGateway(ClientId cliend_id_param, Exchange::ClientRequestLFQueue *client_requests,
                        Exchange::ClientResponseLFQueue *client_responses, std::string ip_param,
                        const std::string &iface, int port_param
                        );
            OrderGateway() = delete;
            OrderGateway(const OrderGateway &) = delete;
            OrderGateway(const OrderGateway &&) = delete;
            OrderGateway &operator=(const OrderGateway &) = delete;
            OrderGateway &operator=(const OrderGateway &&) = delete;

            ~OrderGateway();

            // connects the socket to the exchange and spins up the run() thread
            void start();

            // stops the run() thread
            void stop();

            // thread with infinite loop
            void run();

            // socket will call this after receiving any data (with itself as the "socket" param)
            void recvCallback(TCPSocket *socket, Nanos rx_time) noexcept;

    };

}