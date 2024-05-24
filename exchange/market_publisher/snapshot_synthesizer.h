#pragma once

#include "utils/orderinfo_types.h"
#include "utils/thread_utils.h"
#include "utils/lock_free_queue.h"
#include "utils/macros.h"
#include "utils/multicast_socket.h"
#include "utils/memory_pool.h"
#include "utils/logger.h"

#include "market_publisher/market_update.h"
#include "matching_engine/matching_engine_order.h"

using namespace Common;

namespace Exchange {

    class SnapshotSynthesizer {

        private:
            // LFQ from which the publisher is communicating with the synthesizer thread
            MDPMarketUpdateLFQueue * snapshot_md_updates = nullptr;

            Logger logger;
            std::string time_string;

            volatile bool running = false;

            // socket to send out updates
            MulticastSocket snapshot_socket;

            // snapshot for each ticker, each snapshot is a list from ticker->orders
            std::array<std::array<MEMarketUpdate *, ME_MAX_ORDER_IDs>, ME_MAX_TICKERS> ticker_orders;
            size_t last_inc_seq_num = 0;
            Nanos last_snapshot_time = 0;

            MemPool<MEMarketUpdate> order_pool;

        public:
            SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, 
                                const std::string &interface, const std::string &snapshot_ip, int &snapshot_port
                                ): snapshot_md_updates(market_updates), logger("exchange_snapshot_synthesizer.log"), 
                                snapshot_socket(logger), order_pool(ME_MAX_ORDER_IDs) {
                
                ASSERT(snapshot_socket.init(snapshot_ip, interface, snapshot_port, false) >= 0,
                        "Unable to create snapshot multicast socket, error: " + std::string(std::strerror(errno))
                );
            }

            ~SnapshotSynthesizer() {
                stop();
            }

            void start() {
                running = true;
                ASSERT(Common::createAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this]() {run();}) != nullptr, 
                        "Failed to start snapshotSynthesizer thread"
                );
            }
            
            void stop() {
                running = false;
            }
    };
}