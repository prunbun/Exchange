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
