#pragma once

#include "tcp_socket.h"

namespace Common {
    struct TCPServer {
        
        public:
            int server_file_descriptor = -1;
            TCPSocket listener_socket;

            struct kevent events[1024];
            std::vector<TCPSocket *> sockets;
            std::vector<TCPSocket *> receieve_sockets;
            std::vector<TCPSocket *> send_sockets;
            std::vector<TCPSocket *> disconnected_sockets;

            std::function<void(TCPSocket *, Nanos rx_time)> receive_callback;
            std::function<void()> receive_finished_callback;

            std::string time_string;
            Logger &logger;
    };
}