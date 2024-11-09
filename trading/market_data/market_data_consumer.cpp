#include "market_data_consumer.h"

namespace Trading {

    MarketDataConsumer::MarketDataConsumer(Common::ClientId client_id_param, 
                                Exchange::MEMarketUpdateLFQueue *market_updates_param,
                                const std::string &iface_param,
                                const std::string &snapshot_ip_param, int snapshot_port_param,
                                const std::string &incremental_ip_param, int incremental_port_param):
                                incoming_md_updates(market_updates_param), running(false),
                                logger("trading_market_data_consumer" + std::to_string(client_id_param) + ".log"),
                                incremental_mcast_socket(logger),
                                snapshot_mcast_socket(logger),
                                iface(iface_param), snapshot_ip(snapshot_ip_param), snapshot_port(snapshot_port_param) {
        
        // set up the multicast socket

        // the receive_callback is called during sendAndRecv, so each time the socket hears something,
        // it will trigger the MarketDataConsumer's recv callback
        auto recv_callback = [this](auto socket) {
            recvCallback(socket);
        };
        incremental_mcast_socket.receive_callback = recv_callback;

        // initialize the socket with the ip addr and port, it is a listening socket 
        ASSERT(incremental_mcast_socket.init(incremental_ip_param, iface, incremental_port_param, true) >= 0, 
                "Unable to create incremental mcast socket. error: " + std::string(std::strerror(errno)));

        // have it join the multicast stream at the given ip address
        ASSERT(incremental_mcast_socket.join(incremental_ip_param), 
                "Join failed on:" + std::to_string(incremental_mcast_socket.socket_file_descriptor) + std::string(std::strerror(errno)));

        
        // snapshot socket - this will only be connected as-needed, but the recv will be the same
        snapshot_mcast_socket.receive_callback = recv_callback;
    }

    void MarketDataConsumer::run() {
        logger.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
        
        while (running) {
            // check for updates from the market
            incremental_mcast_socket.sendAndRecv();
            snapshot_mcast_socket.sendAndRecv();
        }
    }

    // so remember that the way a callback works is that the socket that calls this function is the 
    // argument to the function itself (hard to understand at first!)
    void MarketDataConsumer::recvCallback(MulticastSocket *socket) noexcept {

        // we need to first determine if we are receiving snapshot recovery data or an incremental update
        const bool is_snapshot = (socket->socket_file_descriptor == snapshot_mcast_socket.socket_file_descriptor);

        // if we aren't in the recovery phase, we can just ignore any snapshot data
        if (UNLIKELY(is_snapshot && !in_recovery)) {
            socket->next_receive_valid_index = 0;

            logger.log("%:% %() % WARNING: Not expecting snapshot messages.\n",
                __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_str)
            );

            return;
        }

        

    
    }

}