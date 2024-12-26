#include "order_gateway.h"

Trading::OrderGateway::OrderGateway(ClientId cliend_id_param, Exchange::ClientRequestLFQueue *client_requests, 
                                    Exchange::ClientResponseLFQueue *client_responses, std::string ip_param, 
                                    const std::string &iface, int port_param
                                    ): client_id(cliend_id_param), outgoing_requests(client_requests),
                                    incoming_responses(client_responses), ip(ip_param), interface(iface),
                                    port(port_param), logger("trading_order_gateway" + std::to_string(client_id) + ".log"),
                                    tcp_socket(logger) {
    
    tcp_socket.receive_callback = [this](auto socket, auto rx_time) {
        recvCallback(socket, rx_time);
    };

}

Trading::OrderGateway::~OrderGateway() {
    stop();

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(5s);
}

void Trading::OrderGateway::start() {
    running = true;

    // connect the socket
    ASSERT(tcp_socket.connect(ip, interface, port, false) >= 0,
            "Unable to connect to ip:" + ip + " port:" + std::to_string(port) + 
            " on interface:" + interface + " error:" + std::string(std::strerror(errno))
    );

    ASSERT(Common::createAndStartThread(-1, "Trading/OrderGateway", [this]{run();}) != nullptr,
            "Failed to start OrderGateway thread."
    );
}

void Trading::OrderGateway::stop() {
    running = false;
}

void Trading::OrderGateway::run() {

    logger.log("%:% %() %\n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
    );

    // infinite loop
    while (running) {
        // after this func call, we will have data stored in the socket receive buffer, we will read it
        // when the socket calls the recvCallback() from inside this function
        tcp_socket.sendAndReceive();

        // this sends any data pending on the outgoing client requests LFQ
        for (auto client_request = outgoing_requests->getNextRead(); client_request; client_request = outgoing_requests->getNextRead()) {

            logger.log("%:% %() % Sending cid:% seq% %\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                client_id, next_outgoing_seq_number, client_request->toString()
            );

            tcp_socket.send(&next_outgoing_seq_number, sizeof(next_outgoing_seq_number));
            tcp_socket.send(client_request, sizeof(Exchange::MEClientRequest));
            outgoing_requests->updateReadIndex();

            next_outgoing_seq_number++;
        }
    }
}

void Trading::OrderGateway::recvCallback(TCPSocket *socket, Nanos rx_time) noexcept {

    logger.log("%:% %() % Received socket:% len:% %\n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
        socket->socket_file_descriptor, socket->next_receive_valid_index, rx_time
    );

    // we decode the received market data
    if (socket->next_receive_valid_index >= sizeof(Exchange::OMClientResponse)) {

        size_t i = 0;

        for(; i + sizeof(Exchange::OMClientResponse) <= socket->next_receive_valid_index; i += sizeof(Exchange::OMClientResponse)) {

            const Exchange::OMClientResponse * response = reinterpret_cast<const Exchange::OMClientResponse *>(socket->receive_buffer + i);
            logger.log("%:% %() % Received %\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                response->toString()
            );

            // need to make sure the response we get is for this client, else, we ifnore it
            if (response->me_client_response.client_id != client_id) {
                logger.log("%:% %() % ERROR Incorrect client id. ClientId expected:% received:%.\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    client_id, response->me_client_response.client_id
                );
                continue;
            }

            // now we have to verify the sequence number
            if(response->seq_number != next_expected_sequence_number) {
                logger.log("%:% %() % ERROR Incorrect sequence number. ClientId:%. SeqNum expected:% received:%.\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    client_id, next_expected_sequence_number, response->seq_number
                );
                continue;
            }

            // now, it has passed all of our checks
            // we increment our seq num and send it to the trading engine
            ++next_expected_sequence_number;

            auto next_write = incoming_responses->getNextWriteTo();
            *next_write = std::move(response->me_client_response);
            incoming_responses->updateWriteIndex();
        }

        // we have read as much information as we can, so we now update the socket buffer
        // by 'erasing' what we read and shifting the data in the socket over
        // remember, we just read 'i' bytes of data
        memcpy(socket->receive_buffer, socket->receive_buffer + i, socket->next_receive_valid_index - i);
        socket->next_receive_valid_index -= i;
    }

}
