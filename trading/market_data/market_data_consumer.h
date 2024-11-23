#pragma once 
#include <map>

#include "utils/thread_utils.h"
#include "utils/lock_free_queue.h"
#include "utils/macros.h"
#include "utils/multicast_socket.h"

#include "exchange/market_publisher/market_update.h"

namespace Trading {

    /*
        The MarketDataConsumer is responsible for decoding market updates and sending them to the
        trading engine

        Note that if there are errors, the trading engine is stopped while the consumer attempts
        to recover from the loss by filling in the missed data from the snapshot
    */

    class MarketDataConsumer {

        private:
            // data read state
            size_t next_exp_inc_seq_num = 1;
            Exchange::MEMarketUpdateLFQueue *incoming_md_updates = nullptr;

            volatile bool running = false;
            bool in_recovery = false; // used when packet drops are detected

            // logging vars
            std::string time_str;
            Logger logger;
            
            // network params
            Common::MulticastSocket incremental_mcast_socket, snapshot_mcast_socket;
            const std::string iface, snapshot_ip;
            const int snapshot_port;

            // state tracker for received messages (STL Map - uses Red Black Tree)
            typedef std::map<size_t, Exchange::MEMarketUpdate> QueuedMarketUpdates;
            QueuedMarketUpdates snapshot_queued_messages, incremental_queued_msgs;

        public:
            MarketDataConsumer(Common::ClientId client_id_param, 
                                Exchange::MEMarketUpdateLFQueue *market_updates_param,
                                const std::string &iface_param,
                                const std::string &snapshot_ip_param, int snapshot_port_param,
                                const std::string &incremental_ip_param, int incremental_port_param);

            ~MarketDataConsumer() {
                stop();

                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(5s);
            }

            void start() {
                running = true;
                ASSERT(Common::createAndStartThread(-1, "Trading/MarketDataConsumer", [this]() {
                    run();
                }) != nullptr, "Failed to start MarketDataConsumer thread!");
            }

            void stop() {
                running = false;
            }

            void run();
            void recvCallback(MulticastSocket *socket) noexcept;
            void checkSnapshotSync();
            void startSnapshotSync();
            void queueMessage(bool is_snapshot, const Exchange::MDPMarketUpdate * request);


    };

}