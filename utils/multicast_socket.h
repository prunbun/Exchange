#pragma once

#include <functional>

#include "socket_utils.h"

#include "logger.h"

namespace Common {
    constexpr size_t McastBufferSize = 64 * 1024 * 1024;

    struct MulticastSocket {
        MulticastSocket(Logger &logger_param): logger(logger_param) {
            outbound_data.resize(McastBufferSize);
            inbound_data.resize(McastBufferSize);
        }

        // initialize multicast socket to read from or publish to a stream.
        int init(const std::string &ip, const std::string &interface, int port, bool is_listening);

        // add subscription to a multicast stream.
        bool join(const std::string &ip);

        // remove subscription to a multicast stream.
        void leave(const std::string &ip, int port);

        // read data from the buffers and send any stored in the send buffers, if applicable
        bool sendAndRecv() noexcept;

        // copy the given data into the send buffer
        void send(const void *data, size_t len) noexcept;

        int socket_file_descriptor = -1;

        std::vector<char> outbound_data;
        size_t next_send_valid_index = 0;
        std::vector<char> inbound_data;
        size_t next_receive_valid_index = 0;

        std::function<void(MulticastSocket *s)> receive_callback = nullptr;

        std::string time_str;
        Logger &logger;
    };
}