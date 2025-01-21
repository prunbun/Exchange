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
        logger.log("%:% %() % \n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
        
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
            socket->next_receive_valid_index -= i;
        }
    }

    /*
        As context, if we are in recovery mode, essentially, we are going to wipe our client's order
        book and rewrite it using the snapshot we get from the market.

        This means we have phases.
        Phase 1: Make sure we have a complete snapshot
        Phase 2: Stitch on any incremental changes that have come since then onto the snapshot
        Phase 3: Get all of the changes to the client's order book through the LFQ
    */
    void MarketDataConsumer::checkSnapshotSync() {

        // PHASE 1: Check to make sure we have a complete snapshot

        // if we dont have a snapshot at all, there is nothing we can do
        if (snapshot_queued_messages.empty()) {
            return;
        }

        // let's check if we have a start message
        const auto &first_snapshot_message = snapshot_queued_messages.begin()->second;
        if (first_snapshot_message.type != Exchange::MarketUpdateType::SNAPSHOT_START) {
            logger.log("%:% %() % Returning because we have not seen a SNAPSHOT_START yet.\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
            );

            // we need the first entry in the map to be the SNAPSHOT_START
            snapshot_queued_messages.clear();
            return;
        }

        // now, we need to check if we have a sequential snapshot with no gaps
        std::vector<Exchange::MEMarketUpdate> final_events;

        bool have_complete_snapshot = true;
        size_t next_snapshot_seq = 0;
        for (auto &snap_queue_entry : snapshot_queued_messages) {

            // log the map entry
            logger.log("%:% %() % % => %\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                snap_queue_entry.first, snap_queue_entry.second.toString()
            );

            // check sequence number
            if (snap_queue_entry.first != next_snapshot_seq) {
                logger.log("%:% %() % Detected gap in snapshot stream! Expected:% found:% %.\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    next_snapshot_seq, snap_queue_entry.first, snap_queue_entry.second.toString()
                );

                have_complete_snapshot = false;
                break;
            }

            // add it to our final events log (exclude the sentinel starts and ends)
            if (snap_queue_entry.second.type != Exchange::MarketUpdateType::SNAPSHOT_START &&
                snap_queue_entry.second.type != Exchange::MarketUpdateType::SNAPSHOT_END
                ) {

                final_events.push_back(snap_queue_entry.second);
            }

            ++next_snapshot_seq;
        } 

        // we might not have had a complete snapshot, let's check that
        if (!have_complete_snapshot) {
            logger.log("%:% %() % Returning because found gaps in snapshot stream. \n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
            );
            snapshot_queued_messages.clear();
            return;
        }

        // Nice! So now let's check if the snapshot is over or not yet
        const auto &last_snapshot_msg = snapshot_queued_messages.rbegin()->second;
        if (last_snapshot_msg.type != Exchange::MarketUpdateType::SNAPSHOT_END) {
            logger.log("%:% %() % Haven't seen a SNAPSHOT_END message yet! Returning... \n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)            
            );
            
            return;
        }

        // PHASE 2: Check if we have incremental messages we can stitch on after the snapshot ends
        // NOTE: if we have gaps in the remaining incrementals, we need to synchronize them too...
        //       so we will deicide to just let the snapshot grow and keep queueing messages until the snapshot
        //       is big enough such that we get to the part where incrementals have no gaps again 
        // NOTE: For START/END sentinels, the order_id is the latest sequence number for that snapshot

        bool have_complete_incremental = true;
        size_t num_incrementals = 0;
        next_exp_inc_seq_num = last_snapshot_msg.order_id;

        // loop through our queue of incremental messages
        for (auto incr_msg = incremental_queued_msgs.begin(); 
                incr_msg != incremental_queued_msgs.end();
                ++incr_msg
            ) {
            
            // log msg
            logger.log("%:% %() % Next incremental message; next_exp: % vs. seq: % %.\n"
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                next_exp_inc_seq_num, incr_msg->first, incr_msg->second.toString()
            );

            // let's skip those that are before the end of our snapshot, we can just forget about those
            if (incr_msg->first < next_exp_inc_seq_num) {
                continue;
            }

            // now let's make sure there are no gaps
            if (incr_msg->first != next_exp_inc_seq_num) {
                logger.log("%:% %() % Detected gap in incremental stream! Expected: % Found: %. \n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    next_exp_inc_seq_num, incr_msg->first, incr_msg->second.toString()
                );
                have_complete_incremental = false;
                break;
            }

            // cool, let's push this back onto our final_events as long it isn't accidentally a snapshot sentinel
            logger.log("%:% %() % % => %\n",
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                incr_msg->first, incr_msg->second.toString()
            );
            if (incr_msg->second.type != Exchange::MarketUpdateType::SNAPSHOT_START &&
                incr_msg->second.type != Exchange::MarketUpdateType::SNAPSHOT_END
                ) {

                final_events.push_back(incr_msg->second);
                ++next_exp_inc_seq_num;
                ++num_incrementals;
            }
        }

        // we might not have had a complete set of incrementals so let's catch that
        if (!have_complete_incremental) {
            logger.log("%:% %() % Returning because we have gaps in queued incrementals.\n"
                __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
            );

            // at this point, this snapshot isn't enough to recover, so unfortunately, we need a new one
            snapshot_queued_messages.clear();
            return;
        }

        // PHASE 3: Alright! Let's send the snapshot + incremental messages to the client
        for (const auto &final_msg : final_events) {
            auto next_write = incoming_md_updates->getNextWriteTo();
            *next_write = final_msg;
            incoming_md_updates->updateWriteIndex();
        }

        // Cleanup time!
        logger.log("%:% %() % Recovered % snapshot and % incremental orders. \n",
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
            snapshot_queued_messages.size() - 2, num_incrementals
        );

        snapshot_queued_messages.clear();
        incremental_queued_msgs.clear();

        // recovery complete!
        in_recovery = false;

        // close socket file descriptor that is listening to the multicast stream
        snapshot_mcast_socket.leave(snapshot_ip, snapshot_port);
        
    };

    // This is called the first time we are beginning a recovery
    void MarketDataConsumer::startSnapshotSync() {

        // clear the maps used for storing messages during recovery
        snapshot_queued_messages.clear();
        incremental_queued_msgs.clear();

        // now let's connect to the exchange's snapshot endpoint
        ASSERT(snapshot_mcast_socket.init(snapshot_ip, iface, snapshot_port, /* is listening*/ true) >= 0, 
            "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno))
        );

        ASSERT(snapshot_mcast_socket.join(snapshot_ip), 
            "Join failed on:" + std::to_string(snapshot_mcast_socket.socket_file_descriptor) + " error: " +
            std::string(std::strerror(errno)) 
        );
    }

    // this method performs the core synchronization work
    // remember that we can get both incremental AND snapshot messages here
    void MarketDataConsumer::queueMessage(bool is_snapshot, const Exchange::MDPMarketUpdate * request) {
        
        // at this point, we know that when we first started recovery, the queue maps were empty
        // this means that if we ever get duplicates in the map, we have moved onto the next snapshot
        if (is_snapshot) {
            if (snapshot_queued_messages.find(request->seq_number) != snapshot_queued_messages.end()) {
                logger.log("%:% %() % Packet drops on snapshot socket. Received for a 2nd time:%\n",
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                    request->toString()
                );
                snapshot_queued_messages.clear();
            }

            snapshot_queued_messages[request->seq_number] = request->me_market_update;

        } else { // incremental
            incremental_queued_msgs[request->seq_number] = request->me_market_update;
        }

        logger.log("%:% %() % size snapshot:% incremental:% % => %\n",
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
            snapshot_queued_messages.size(), request->seq_number, request->toString()
        );

        // at this point, we might be able to stop the recovery phase
        checkSnapshotSync();
    }

}