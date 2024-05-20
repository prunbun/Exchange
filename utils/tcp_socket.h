#pragma once

#include <functional>
#include "socket_utils.h"
#include "logger.h"
#include <iostream>

namespace Common {

    constexpr size_t TCPBufferSize = 64 * 1024 * 1024;

    struct TCPSocket {
        // socket
        int socket_file_descriptor = -1;

        // send and receive buffers
        char *send_buffer = nullptr;
        size_t next_send_valid_index = 0;
        char *receive_buffer = nullptr;
        size_t next_receive_valid_index = 0;

        // status bools
        bool send_socket_disconnected = false;
        bool receive_socket_disconnected = false;

        struct sockaddr_in inInAddr;

        // This is called a 'callback' in which we can store a function that matches the given template
        // once we receive a message, this function can be called, fairly common pattern in socket programming
        std::function<void(TCPSocket *socket, Nanos rx_time)> receive_callback;

        // logging utils
        std::string time_string;
        Logger &logger;
        void defaultCallback(TCPSocket *socket, Nanos rx_time) noexcept {
            logger.log("% % %() % TCPSocket::defaultCallback() socket:% len:% rx:% \n",
                __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_string), socket->socket_file_descriptor, socket->next_receive_valid_index, rx_time
            );
        }

        explicit TCPSocket(Logger &logger_obj): logger(logger_obj) {
            send_buffer = new char[TCPBufferSize];
            receive_buffer = new char[TCPBufferSize];
            receive_callback = [this](auto socket, auto rx_time) {
                defaultCallback(socket, rx_time);
            };
        }

        void destroy() noexcept {
            close(socket_file_descriptor);
            socket_file_descriptor = -1; // similar to setting stuff to nullptr
        }
        ~TCPSocket() {
            destroy();

            delete[] send_buffer;
            send_buffer = nullptr;
            delete[] receive_buffer;
            receive_buffer = nullptr;
        }

        TCPSocket() = delete;
        TCPSocket(const TCPSocket &) = delete;
        TCPSocket(const TCPSocket &&) = delete;
        TCPSocket &operator=(const TCPSocket &) = delete;
        TCPSocket &operator=(const TCPSocket &&) = delete;


        /* MEMBER FUNCTIONS */

        // creates a TCP Socket that either listens on or coneects to the given ip, interface, and port
        int connect(const std::string &ip, const std::string &interface, int port, bool is_listening);

        // Publishes all data in send buffers and executes the callback if data exists in the receive buffer
        bool sendAndReceive() noexcept;

        // Writes provided data to send buffer
        void send(const void *data, size_t length) noexcept;
    };

}