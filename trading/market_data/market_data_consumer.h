#pragma once 
#include <map>

#include "utils/thread_utils.h"
#include "utils/lock_free_queue.h"
#include "utils/macros.h"
#include "utils/multicast_socket.h"

#include "exchange/market_publisher/market_update.h"

namespace Trading {

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



    };

}