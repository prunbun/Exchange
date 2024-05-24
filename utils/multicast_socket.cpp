#include "multicast_socket.h"

namespace Common {

    int MulticastSocket::init(const std::string &ip, const std::string &interface, int port, bool is_listening) {
        socket_file_descriptor = createSocket(logger, ip, interface, port, true, false, is_listening, 0, false);
        return socket_file_descriptor;
    }

    bool MulticastSocket::join(const std::string &ip) {
        return Common::join(socket_file_descriptor, ip);
    }

    void MulticastSocket::leave(const string &, int) {
        close(socket_file_descriptor);
        socket_file_descriptor = -1;
    }

    bool MulticastSocket::sendAndRecv() noexcept {
        const ssize_t n_rcv = recv(socket_file_descriptor, inbound_data.data() + next_receive_valid_index, McastBufferSize - next_receive_valid_index, MSG_DONTWAIT);
        if (n_rcv > 0) {
            next_receive_valid_index += n_rcv;
            logger.log("%:% %() % read socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string), socket_file_descriptor,
                        next_receive_valid_index);
            receive_callback(this);
        }

        // Publish market data in the send buffer to the multicast stream.
        if (next_send_valid_index > 0) {
            ssize_t n = ::send(socket_file_descriptor, outbound_data.data(), next_send_valid_index, MSG_DONTWAIT | MSG_NOSIGNAL);

            logger.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string), socket_file_descriptor, n);
        }
        next_send_valid_index = 0;

        return (n_rcv > 0);
    }

    void MulticastSocket::send(const void *data, size_t len) noexcept {
        memcpy(outbound_data.data() + next_send_valid_index, data, len);
        next_send_valid_index += len;
        ASSERT(next_send_valid_index < McastBufferSize, "Mcast socket buffer filled up and sendAndRecv() not called.");
    }
}