#include "../logger.h"
#include "../tcp_server.h"
#include "../time_utils.h"
#include <iostream>

int main() {
    using namespace Common;

    // create logger and helpers for timing and tracking data flow
    std::string time_string;
    Logger logger("server_testing.log");
    
    auto tcpServerRecvCallback = [&](TCPSocket *socket, Nanos rx_time) noexcept {
        logger.log("TCPServer::tcpServerRecvCallback() socket:% len:% rx:% \n",
            socket->socket_file_descriptor, socket->next_receive_valid_index, rx_time
        );

        const std::string reply = "TCPServer received msg: " + std::string(socket->receive_buffer, socket->next_receive_valid_index);
        socket->next_receive_valid_index = 0;
        socket->send(reply.data(), reply.length());
    };
    
    auto tcpServerRecvFinishedCallback = [&]() noexcept {
        logger.log("TCPServer::tcpServerRecvFinishedCallback() \n");
    };
    
    auto tcpClientRecvCallback = [&](TCPSocket *socket, Nanos rx_time) noexcept {
        const std::string recv_msg = std::string(socket->receive_buffer, socket->next_receive_valid_index);
        socket->next_receive_valid_index = 0;

        logger.log("TCPServer::tcpClientRecvCallback() socket:% len:% rx:% msg:% \n",
            socket->socket_file_descriptor, socket->next_receive_valid_index, rx_time, recv_msg
        );
    };

    
    // create, initialize, and connect the clients
    const std::string interface = "lo";
    const std::string ip = "127.0.0.1";
    const int port = 12345;
    
    logger.log("creating TCPServer on interface:% port:% \n", interface, port);
    TCPServer server(logger);
    server.receive_callback = tcpServerRecvCallback;
    server.receive_finished_callback = tcpServerRecvFinishedCallback;
    server.listen(interface, port);
    
    // --

    std::vector<TCPSocket *> clients(1);
    
    for (size_t i = 0; i < clients.size(); ++i) {
        clients[i] = new TCPSocket(logger);
        clients[i]->receive_callback = tcpClientRecvCallback;

        logger.log("Connecting TCPClient - [%] on ip:% interface:% port:% \n", i, ip, interface, port);
        clients[i]->connect(ip, interface, port, false);
        server.poll();
    }
    
    using namespace std::literals::chrono_literals;
    
    for (auto iter = 0; iter < 5; ++iter) {
        for (size_t i = 0; i < clients.size(); ++i) {
            const std::string client_msg = "Client-[" + std::to_string(i) + "] : Sending " + std::to_string(iter * 100 + i);
            logger.log("Sending TCPClient-[%] % \n", i, client_msg);
            clients[i]->send(client_msg.data(), client_msg.length());
            clients[i]->sendAndReceive();

            std::this_thread::sleep_for(500ms);
            server.poll();
            server.sendAndReceive(); 
        }
    }
    
    return 0;
};