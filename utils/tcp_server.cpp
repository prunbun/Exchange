#include "tcp_server.h"

namespace Common {

    /*
        we initialize a kqueue and add this server's socket to the kqueue
        a kqueue is a way on how we can listen on many sockets at once and process events
    */
    void TCPServer::listen(const std::string &interface, int port) {

        destroy();
        kqueue_file_descriptor = kqueue();
        ASSERT(kqueue_file_descriptor >= 0, "kqueue() failed error: " + std::string(std::strerror(errno)));

        ASSERT(listener_socket.connect("", interface, port, true) >= 0,
        "Listener socket failed to connect. interface: " + interface + " port: " + std::to_string(port) + 
        " error: " + std::string(std::strerror(errno)));

        ASSERT(kqueue_add(&listener_socket),
        "Failed to add socket fd to kqueue. error: " + std::string(std::strerror(errno)));
    }

    // update all data structures with new state of incoming socket events
    void TCPServer::poll() noexcept {
        const int max_events = 1 + sockets.size();
        // check if any sockets need to be removed from the kqueue
        for (auto socket : disconnected_sockets) {
            del(socket);
        }

        // fetch a list of all the events we received
        const int n = kevent(kqueue_file_descriptor, nullptr, 0, events, max_events, nullptr);

        bool have_new_connection = false;
        for (int i = 0; i < n; ++i) {
            struct kevent &event = events[i];
            auto socket = reinterpret_cast<TCPSocket *>(event.udata);

            if (event.filter == EVFILT_READ) {
                if (socket == &listener_socket) {
                    // indicates we have gotten a new connection, need to make a new receiving socket to process this
                    logger.log("%:% %() % EVFILT_READ listener_socket:% \n", 
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                    socket->socket_file_descriptor
                    );
                    have_new_connection = true;
                    continue;
                }

                logger.log("%:% %() % EVFILT_READ listener_socket:% \n", 
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                socket->socket_file_descriptor
                );

                // put the socket into our receiver logs
                if (std::find(receieve_sockets.begin(), receieve_sockets.end(), socket) == receieve_sockets.end()) {
                    receieve_sockets.push_back(socket);
                }
            }

            // new socket for sending data
            if (event.filter == EVFILT_WRITE) {
                logger.log("%:% %() % EVFILT_WRITE listener_socket:% \n", 
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                socket->socket_file_descriptor
                );

                if (std::find(send_sockets.begin(), send_sockets.end(), socket) == send_sockets.end()) {
                    send_sockets.push_back(socket);
                }
            }

            // these sockets have an issue and need to be deactivated
            if (event.flags & (EV_ERROR | EV_EOF)) {
                logger.log("%:% %() % EV_ERROR or EV_EOF listener_socket:% \n", 
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                socket->socket_file_descriptor
                );

                if (std::find(disconnected_sockets.begin(), disconnected_sockets.end(), socket) == disconnected_sockets.end()) {
                    disconnected_sockets.push_back(socket);
                }
            }
        }

        // accept the new connection, create a socket for it, and register it in our kqueue and data structures
        while (have_new_connection) {
            logger.log("%:% %() % new connection! \n", 
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string)
            );

            sockaddr_storage addr;
            socklen_t addr_len = sizeof(addr);
            int file_descriptor = accept(listener_socket.socket_file_descriptor, reinterpret_cast<sockaddr *>(&addr), &addr_len);
            if (file_descriptor == -1) {
                break;
            }

            ASSERT(setNonBlocking(file_descriptor) && setNoDelay(file_descriptor),
                "Failed to set non-blocking or no-delay on socket: " + std::to_string(file_descriptor)
            );
            logger.log("%:% %() % accepted socket:% \n", 
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
            file_descriptor
            );

            TCPSocket *accepted_socket = new TCPSocket(logger);
            accepted_socket->socket_file_descriptor = file_descriptor;
            accepted_socket->receive_callback = receive_callback;
            ASSERT(kqueue_add(accepted_socket), "unable to add socket. error: " + std::string(std::strerror(errno)));
            if (std::find(sockets.begin(), sockets.end(), accepted_socket) == sockets.end()) {
                sockets.push_back(accepted_socket);
            }
            if (std::find(receieve_sockets.begin(), receieve_sockets.end(), accepted_socket) == receieve_sockets.end()) {
                receieve_sockets.push_back(accepted_socket);
            }
        }
    }

    // for all read sockets, read all data, if available, trigger callback if any data was read
    void TCPServer::sendAndReceive() noexcept {

        bool data_read = false;
        for (auto socket : receieve_sockets) {
            if (socket->sendAndReceive()) {
                data_read = true;
            }
        }
        if (data_read) {
            receive_finished_callback();
        }

        // for all send sockets, send any data we have stored in their send buffers
        for (auto socket : send_sockets) {
            socket->sendAndReceive();
        }
    }

}