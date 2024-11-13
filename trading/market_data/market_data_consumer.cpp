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
    // when we get data on a socket we are listening to, this function will be called
    // when in recovery, we will start/continue that process
    // else we will extract the MEMarketUpdate and send it to the client order book
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

        // otherwise lets read the data
        if (socket->next_receive_valid_index >= sizeof(Exchange::MDPMarketUpdate)) {
            size_t i = 0;
            for (; i + sizeof(Exchange::MDPMarketUpdate) <= socket->next_receive_valid_index; i += sizeof(Exchange::MDPMarketUpdate)) {
                auto request = reinterpret_cast<const Exchange::MDPMarketUpdate *>(socket->inbound_data.data() + i);

                logger.log("%:% %() % Received % socket len:% %\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    (is_snapshot ? "snapshot" : "incremental"), sizeof(Exchange::MDPMarketUpdate),
                    request->toString()
                );

                // so it is possible we are already in recovery OR we saw a gap in the sequence nums
                const bool already_in_recovery = in_recovery;
                in_recovery = (already_in_recovery || request->seq_number != next_exp_inc_seq_num);

                if (UNLIKELY(in_recovery)) {

                    // if we were not in recovery before and we just saw a mismatch seq num
                    // we now have to activate recovery mode and listen to the snapshot broadcast
                    if(UNLIKELY(!already_in_recovery)) {
                        logger.log("%:% %() % Packet drops on % socket! SeqNum expected:% received:% \n",
                            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                            (is_snapshot ? "snapshot" : "incremental"), next_exp_inc_seq_num, request->seq_number
                        );

                        // begin recovery phase - subscribe to snapshot multicast
                        startSnapshotSync();
                    }

                    // we'll send this message to processing, it could be snapshot OR incremental data
                    queueMessage(is_snapshot, request);

                } else if (!is_snapshot) { 
                    // only care if it is incremental updates
                    // note this is 'normal' operation when not in recovery

                    // acknowledge that we got an incremental update
                    logger.log("%:% %() % Incremental Request: %\n",
                        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                        request->toString()
                    );
                    ++next_exp_inc_seq_num;

                    // great! let's send it to the client's order book
                    Exchange::MEMarketUpdate * next_write = incoming_md_updates->getNextWriteTo();
                    *next_write = std::move(request->me_market_update);
                    incoming_md_updates->updateWriteIndex();
                }
            }

            // let's update the socket's state based on how much we data we read
            memcpy(socket->inbound_data.data(), socket->inbound_data.data() + i, socket->next_receive_valid_index - i);
            socket->next_receive_valid_index -= i
        }
    }

}