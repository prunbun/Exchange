#include "tcp_socket.h"

namespace Common {
    // creates a TCP Socket that either listens on or coneects to the given ip, interface, and port
    int TCPSocket::connect(const std::string &ip, const std::string &interface, int port, bool is_listening) {
        destroy();
        socket_file_descriptor = createSocket(logger, ip, interface, port, false, false, is_listening, 0, true); // tcp connection

        inInAddr.sin_addr.s_addr = INADDR_ANY;
        inInAddr.sin_port = htons(port);
        inInAddr.sin_family = AF_INET;

        return socket_file_descriptor;
    }

    // Publishes all data in send buffers and executes the callback if data exists in the receive buffer
    bool TCPSocket::sendAndReceive() noexcept {
        // first we set up a buffer that can receive messages from a socket
        // we also create something called a "scatter-gather list" to keep memory together even if it is not contiguous
        char ctrl[CMSG_SPACE(sizeof(struct timeval))];
        struct cmsghdr *cmsg = (struct cmsghdr *) &ctrl;

        struct iovec iov;
        iov.iov_base = receive_buffer + next_receive_valid_index;
        iov.iov_len = TCPBufferSize - next_receive_valid_index;

        msghdr msg;
        msg.msg_control = ctrl;
        msg.msg_controllen = sizeof(ctrl);
        msg.msg_name = &inInAddr;
        msg.msg_namelen = sizeof(inInAddr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        // where we receive the message from the socket
        // Note to readers: This code is kind of complex so I had to consult other resources for coding help
        // I am not fully confident on the code that deals with timing and some of the settings for the msg fields
        // UPDATE: this timing methods works well on linux and for udp in general, tcp it doesn't work as well, for now, we will just use our user time instead
        const auto n_rcv = recvmsg(socket_file_descriptor, &msg, MSG_DONTWAIT);
        if (n_rcv > 0) {
            next_receive_valid_index += n_rcv;

            /*
            NOTE: For now, we will use our implementation of user time clocking as it works better for tcp

            // Nanos kernel_time = 0;
            // struct timeval time_kernel;
            // if (cmsg->cmsg_level == SOL_SOCKET
            //     && cmsg->cmsg_type == SCM_TIMESTAMP
            //     && cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel))
            // ) {
            //     memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
            //     kernel_time = time_kernel.tv_sec * NANOS_TO_SECONDS + time_kernel.tv_usec * NANOS_TO_MICROS;
            // }
            */

            const auto user_time = getCurrentNanos();

            logger.log("%: % %() % read socket: % len:% utime:% \n",
                __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_string),
                socket_file_descriptor, next_receive_valid_index, user_time
            );

            receive_callback(this, user_time);
        }

        ssize_t n_send = std::min(TCPBufferSize, next_send_valid_index);
        while (n_send > 0) {
            auto n_send_this_msg = std::min(static_cast<ssize_t>(next_send_valid_index), n_send);
            const int flags = MSG_DONTWAIT | MSG_NOSIGNAL;
            auto n = ::send(socket_file_descriptor, send_buffer, n_send_this_msg, flags);

            if (UNLIKELY(n < 0)) {
                if (!wouldBlock()) {
                    send_socket_disconnected = true;
                }
                break;
            }

            logger.log("%:% %() % send socket:% len:% \n",
                __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_string), socket_file_descriptor, n
            );
            
            n_send -= n;
            ASSERT(n == n_send_this_msg, "Don't support partial send lengths yet.");
        }
        next_send_valid_index = 0;

        return (n_rcv > 0);
    }

    // Writes provided data to send buffer
    void TCPSocket::send(const void *data, size_t length) noexcept {
        if (length > 0) {
            memcpy(send_buffer + next_send_valid_index, data, length);
            next_send_valid_index += length;
        }

        return;
    }
}