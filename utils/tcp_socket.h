#pragma once

#include <functional>
#include "socket_utils.h"
#include "logger.h"

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
        int connect(const std::string &ip, const std::string &interface, int port, bool is_listening) {
            destroy();
            socket_file_descriptor = createSocket(logger, ip, interface, port, false, false, is_listening, 0, true); // tcp connection

            inInAddr.sin_addr.s_addr = INADDR_ANY;
            inInAddr.sin_port = htons(port);
            inInAddr.sin_family = AF_INET;

            return socket_file_descriptor;
        }

        // Publishes all data in send buffers and executes the callback if data exists in the receive buffer
        bool sendAndReceive() noexcept {
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
            const auto n_rcv = recvmsg(socket_file_descriptor, &msg, MSG_DONTWAIT);
            if (n_rcv > 0) {
                next_receive_valid_index += n_rcv;

                Nanos kernel_time = 0;
                struct timeval time_kernel;
                if (cmsg->cmsg_level == SOL_SOCKET
                    && cmsg->cmsg_type == SCM_TIMESTAMP
                    && cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel))
                ) {
                    memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
                    kernel_time = time_kernel.tv_sec * NANOS_TO_SECONDS + time_kernel.tv_usec * NANOS_TO_MICROS;
                }

                const auto user_time = getCurrentNanos();

                logger.log("%: % %() % read socket: % len:% utime:% ktime:% diff:% \n",
                    __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_string),
                    socket_file_descriptor, next_receive_valid_index, user_time,
                    kernel_time, (user_time - kernel_time)
                );

                receive_callback(this, kernel_time);
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
        void send(const void *data, size_t length) noexcept {
            if (length > 0) {
                memcpy(send_buffer + next_send_valid_index, data, length);
                next_send_valid_index += length;
            }

            return;
        }
    };

}