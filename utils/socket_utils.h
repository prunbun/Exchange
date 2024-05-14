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
        bool op_successful = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        return (op_successful != -1);
    }

    bool setNoDelay (int fd) {
        // this method disables something called Nagle's algorithm, that intentionally puts a delay in between packets to avoid dropping them
        // but for low-latency, this slows down the application
        int one = 1;

        // as a clarification, a 'file descriptor' or fd here, is a identifier given by the OS to a particular socket, file etc.
        bool op_successful = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one));
        return (op_successful != -1);
    }

    bool setSOTimestamp (int fd) {
        // this method records the timestamp at which a packet arrives at the socket 

        int one = 1;
        bool op_successful = setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one));
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
        
        bool op_successful = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<void *>(&ttl), sizeof(ttl));
        return (op_successful != -1);
    }

    bool setTTL(int fd, int ttl) {
        bool op_successful = setsockopt(fd, IPPROTO_IP, IP_TTL, reinterpret_cast<void *>(&ttl), sizeof(ttl));
        return (op_successful != -1);
    }

    bool join(int fd, const std::string &ip, const std::string &interface, int port) {
        // subscribes the given fd to a multicast stream on a given interface
        const ip_mreq mreq{{inet_addr(ip.c_str())}, {htonl(INADDR_ANY)}};
        return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != -1);
    }

    int createSocket(Logger &logger, const std::string &t_ip, const std::string &interface, int port, bool is_udp, bool is_blocking, bool is_listening, int ttl, bool needs_so_timestamp);
}