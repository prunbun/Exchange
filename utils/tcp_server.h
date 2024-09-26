#pragma once

#include "tcp_socket.h"

namespace Common {
    struct TCPServer {
        
        public:
            int kqueue_file_descriptor = -1;
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

            auto defaultRecvCallback(TCPSocket *socket, Nanos rx_time) noexcept {
                logger.log("%:% %() % TCPServer::defaultRecvCallback() socket:% len:% rx:% \n", 
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string),
                socket->socket_file_descriptor, socket->next_receive_valid_index, rx_time
                );
            }

            auto defaultRecvFinishedCallback() noexcept {
                logger.log("%:% %() % TCPServer::defaultRecvFinishedCallback() \n", 
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_string)
                );
            }

            // notice that the code will call TCPSocket(Logger &logger) constructor instead of doing a copy constructor
            explicit TCPServer(Logger &logger_obj) : listener_socket(logger_obj), logger(logger_obj) {
                receive_callback = [this](auto socket, auto rx_time) {
                    defaultRecvCallback(socket, rx_time);
                };
                receive_finished_callback = [this]() {
                    defaultRecvFinishedCallback();
                };
            }

            ~TCPServer() {
                std::unordered_set<TCPSocket *> unique_sockets;

                // Insert pointers from all vectors into the set
                for (auto socket : sockets) {
                    unique_sockets.insert(socket);
                }
                for (auto socket : receieve_sockets) {
                    unique_sockets.insert(socket);
                }
                for (auto socket : send_sockets) {
                    unique_sockets.insert(socket);
                }
                for (auto socket : disconnected_sockets) {
                    unique_sockets.insert(socket);
                }

                // Now delete each unique socket once
                for (auto socket : unique_sockets) {
                    delete socket;
                }
            }

            auto destroy() {
                close(kqueue_file_descriptor);
                kqueue_file_descriptor = -1;
                listener_socket.destroy();
            }

            TCPServer() = delete;
            TCPServer(const TCPServer &) = delete;
            TCPServer(const TCPServer &&) = delete;
            TCPServer &operator=(const TCPServer &) = delete;
            TCPServer &operator=(const TCPServer &&) = delete;

            /*
                we initialize a kqueue and add this server's socket to the kqueue
                a kqueue is a way on how we can listen on many sockets at once and process events
            */
            void listen(const std::string &interface, int port);

            bool kqueue_add(TCPSocket *socket) {
                // adds a socket to the kqueue
                struct kevent ev;
                EV_SET(&ev, socket->socket_file_descriptor, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, reinterpret_cast<void *>(socket));
                return (kevent(kqueue_file_descriptor, &ev, 1, nullptr, 0, nullptr) != -1);
            }

            bool kqueue_del(TCPSocket *socket) {
                // deletes a socket from the kqueue
                struct kevent ev;
                EV_SET(&ev, socket->socket_file_descriptor, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                return (kevent(kqueue_file_descriptor, &ev, 1, nullptr, 0, nullptr) != -1);
            }

            void del(TCPSocket *socket) {
                // deletes a socket from the kqueue and removes it from our data structures
                kqueue_del(socket);
                sockets.erase(std::remove(sockets.begin(), sockets.end(), socket), sockets.end());
                receieve_sockets.erase(std::remove(receieve_sockets.begin(), receieve_sockets.end(), socket), receieve_sockets.end());
                send_sockets.erase(std::remove(send_sockets.begin(), send_sockets.end(), socket), send_sockets.end());
                delete socket;
            }

            // update all data structures with new state of incoming socket events
            void poll() noexcept; 

            // for all read sockets, read all data, if available, trigger callback if any data was read
            void sendAndReceive() noexcept;
    };
}