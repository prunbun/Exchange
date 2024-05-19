#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <sys/event.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/event.h>

#include "logger.h"


namespace Common {
    constexpr int MaxTCPServerBacklog = 1024;

    // take an interface name as a string and return the associated ip address
    std::string getIFaceIP(const std::string &interface) {
        char buf[NI_MAXHOST] = {'\0'}; // NI_MAXHOST is the length of the longest hostname possible
        ifaddrs *ifaddr = nullptr; // this will be a linked list

        // getifaddrs returns a linked list of interfaces and information about them
        if (getifaddrs(&ifaddr) != -1) { 

            // accordingly, let's loop over the interfaces to get the ip address we want
            // and store it in buf
            for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if ( ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET
                    && ifa->ifa_name == interface) {
                        getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
                        break;
                    }
            }

            freeifaddrs(ifaddr); // library function that will free the linked list
        }

        return buf;
    }

    bool setNonBlocking (int fd) {
        // here, we don't want the socket to wait until it has data to read, otherwise the application blocks

        const auto flags = fcntl(fd, F_GETFL, 0); // get socket file descriptor flags

        // unable to retrieve flags
        if (flags == -1) {
            return false;
        }

        // if it is already non-blocking, don't do anything
        if (flags & O_NONBLOCK) {
            return true;
        }

        // otherwise, set the flags to nonblocking using an OR with O_NONBLOCK
        int op_successful = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        return (op_successful != -1);
    }

    bool setNoDelay (int fd) {
        // this method disables something called Nagle's algorithm, that intentionally puts a delay in between packets to avoid dropping them
        // but for low-latency, this slows down the application
        int one = 1;

        // as a clarification, a 'file descriptor' or fd here, is a identifier given by the OS to a particular socket, file etc.
        int op_successful = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one));
        return (op_successful != -1);
    }

    bool setSOTimestamp (int fd) {
        // this method records the timestamp at which a packet arrives at the socket 

        int one = 1;
        int op_successful = setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one));
        return (op_successful != -1);
    }

    bool wouldBlock() {
        // checks whether a socket operation would block or not

        // errno is a global variable that is set when functions execute/fail for the programmer to know what is going on
        // examples are like EINVAL (invalid argument) EIO (input/output error)

        return (errno == EWOULDBLOCK || errno == EINPROGRESS);
    }

    bool setMcastTTL(int fd, int ttl) {
        // TTL represents the number of hops a packet can take from sender to receiver
        // this is generally good practice for security reasons, reducing network congestion, and stopping cycles
        
        int op_successful = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<void *>(&ttl), sizeof(ttl));
        return (op_successful != -1);
    }

    bool setTTL(int fd, int ttl) {
        int op_successful = setsockopt(fd, IPPROTO_IP, IP_TTL, reinterpret_cast<void *>(&ttl), sizeof(ttl));
        return (op_successful != -1);
    }

    bool join(int fd, const std::string &ip, const std::string &interface, int port) {
        // subscribes the given fd to a multicast stream on a given interface
        const ip_mreq mreq{{inet_addr(ip.c_str())}, {htonl(INADDR_ANY)}};
        return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
    }

    int createSocket(Logger &logger, const std::string &t_ip, const std::string &interface, int port, bool is_udp, bool is_blocking, bool is_listening, int ttl, bool needs_so_timestamp) {
        // PART 1: we have to create the addrinfo struct that is used by the system to create a socket
        
        // 1.1 string that will hold the timestamp of when we create this socket
        std::string time_string;

        // 1.2 retrieves the ip address we want this socket to listen on
        const std::string ip = t_ip.empty() ? getIFaceIP(interface) : t_ip;

        logger.log("%:% %() % ip:% interface:% port:% is_udp:% is_blocking:% is_listening:% ttl:% SOtime:% \n", 
            __FILE__, __LINE__, __FUNCTION__, 
            Common::getCurrentTimeStr(&time_string),
            ip, interface, port, is_udp, is_blocking, is_listening, ttl, needs_so_timestamp
        );

        // 1.3 populating the addrinfo hints struct that will be args into getaddrinfo, which will then be used to create the socket
        addrinfo hints{}; // <- the curly braces initialize all fields to their default values
        hints.ai_family = AF_INET;
        hints.ai_socktype = is_udp ? SOCK_DGRAM : SOCK_STREAM;
        hints.ai_protocol = is_udp ? IPPROTO_UDP : IPPROTO_TCP;

            // so generally, the system does a DNS query to resolve hostnames and some other lookups to resolve service port number
            // this code tells getaddrinfo to treat the inputs as the numeric values and not do a lookup
        if (std::isdigit(ip.c_str()[0])) {
            hints.ai_flags |= AI_NUMERICHOST;
        }
        hints.ai_flags |= AI_NUMERICSERV;

        // 1.4 pass in the hints to getaddrinfo and generate the socket creation args
        addrinfo *result = nullptr;
        const auto possible_err = getaddrinfo(ip.c_str(), std::to_string(port).c_str(), &hints, &result);

        if (possible_err) {
            logger.log("getaddrinfo() failed, info: error: % errno: % \n", gai_strerror(possible_err), strerror(errno));
        }

        
        // PART 2: Create the socket using the params
        
        // getinfoaddr creates a linked list of socket args for all the different interfaces
        // 2.1 we use a for loop to traverse this list
        int socket_file_descriptor = -1;
        int one = 1;
        for (addrinfo *socket_args = result; socket_args; socket_args = socket_args->ai_next) {
            
            // 2.2 try to create the socket
            socket_file_descriptor = socket(socket_args->ai_family, socket_args->ai_socktype, socket_args->ai_protocol);
            if (socket_file_descriptor == -1) {
                logger.log("socket() function failed. errno: % \n", strerror(errno));
                return -1;
            }

            // 2.3 if the socket was created successfully, we should set it to be non-blocking and have no packet delays
            if (!is_blocking) {
                if (!setNonBlocking(socket_file_descriptor)) {
                    logger.log("setNonBlocking failed. errno: % \n", strerror(errno));
                    return -1;
                }

                // note that the delays only occur during TCP connections to ensure reliable delivery
                if (!is_udp && !setNoDelay(socket_file_descriptor)) {
                    logger.log("setNoDelay failed. errno: % \n", strerror(errno));
                    return -1;
                }
            }

            // 2.4 now, we have to split between two types of sockets: listening and connecting (connecting with target ip)
            // note that connect() would return 0 if it worked
            if (!is_listening && 
                connect(socket_file_descriptor, socket_args->ai_addr, socket_args->ai_addrlen) == -1 &&
                !wouldBlock()
            ) {
                logger.log("connect() failed. errno: % \n", strerror(errno));
                return -1;
            }

            constexpr int MaxTCPServerBacklog = 1024;
            if (is_listening && setsockopt(socket_file_descriptor, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void *>(&one), sizeof(one)) == -1) {
                logger.log("setsockopt() failed. errno: % \n", strerror(errno));
                return -1;
            }
            if (is_listening && bind(socket_file_descriptor, socket_args->ai_addr, socket_args->ai_addrlen) == -1) {
                logger.log("bind() failed. errno: % \n", strerror(errno));
                return -1;
            }
            if (!is_udp && is_listening && listen(socket_file_descriptor, MaxTCPServerBacklog) == -1) {
                logger.log("listen() failed. errno: % \n", strerror(errno));
                return -1;
            }

            // 2.5 set the TTL for packets if it is provided and let the socket know to register timestamps for packets
            if (is_udp && ttl) {
                // this checks the prefix since only a certain range of addr are for multicast
                const bool is_multicast = atoi(ip.c_str()) & 0xe0; 

                if (is_multicast && !setMcastTTL(socket_file_descriptor, ttl)) {
                    logger.log("setMcastTTL() failed. errno: % \n", strerror(errno));
                    return -1;
                }
                if (!is_multicast && !setTTL(socket_file_descriptor, ttl)) {
                    logger.log("setTTL() failed. errno: % \n", strerror(errno));
                    return -1;
                }
            }

            if (needs_so_timestamp && !setSOTimestamp(socket_file_descriptor)) {
                logger.log("setSOTimestamp() failed. errno: % \n", strerror(errno));
                return -1;
            }

        }

        // PART 3: free memory and return, remember that many things we do are on the stack, but getaddrinfo is on the heap
        if (result) {
            freeaddrinfo(result);
        }

        return socket_file_descriptor;
    }
}