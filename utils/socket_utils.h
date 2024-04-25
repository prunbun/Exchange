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

    bool setNonBlocking (int fd);
    bool setNoDelay (int fd);
    bool setNoTimestamp (int fd);
    bool wouldBlock();
    bool setMcastTTL(int fd, int ttl);
    bool setTTL(int fd, int ttl);
    bool join(int fd, const std::string &ip, const std::string &interface, int port);
    int createSocket(Logger &logger, const std::string &t_ip, const std::string &interface, int port, bool is_udp, bool is_blocking, bool is_listening, int ttl, bool needs_so_timestamp);
}