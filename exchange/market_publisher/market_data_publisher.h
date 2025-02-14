#pragma once

#include <functional>

#include "market_publisher/snapshot_synthesizer.h"
#include "market_publisher/market_update.h"
#include "../../utils/logger.h"
#include "../../utils/multicast_socket.h"
#include "../../utils/exchange_limits.h"

namespace Exchange {

    /*
        this file contains the publisher that sends out order book updates to all market participants
        - we have one portion of the publisher that sends out the m. e. updates
        - the other part puts together a snapshot of the order book that is published less frequently
    */
    class MarketDataPublisher {

        private:
            size_t next_inc_seq_number = 1;
            MEMarketUpdateLFQueue * outgoing_md_updates = nullptr;

            MDPMarketUpdateLFQueue snapshot_md_updates;

            volatile bool running = false;

            std::string time_str;
            Logger logger;

            Common::MulticastSocket incremental_socket;

            SnapshotSynthesizer * snapshot_synthesizer = nullptr;

        public:
            MarketDataPublisher(MEMarketUpdateLFQueue * market_updates, const std::string &interface,
                                const std::string snapshot_ip, int snapshot_port, 
                                const std::string &incremental_ip, int incremental_port
                                ): outgoing_md_updates(market_updates), snapshot_md_updates(ME_MAX_MARKET_UPDATES),
                                running(false), logger("exchange_market_data_publisher.log"), incremental_socket(logger) {
                
                ASSERT(incremental_socket.init(incremental_ip, interface, incremental_port, false) >= 0,
                        "Unable to create incremental multicast socket. error:" + std::string(std::strerror(errno))
                );
                snapshot_synthesizer = new SnapshotSynthesizer(&snapshot_md_updates, interface, snapshot_ip, snapshot_port);
            }

            ~MarketDataPublisher() {
                stop();

                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(5s);

                delete snapshot_synthesizer;
                snapshot_synthesizer = nullptr;
            }

            void start() {
                running = true;
                ASSERT(Common::createAndStartThread(-1, "Exchange/MarketDataPublisher", [this]() { run(); }) != nullptr, 
                        "Failed to start MarketData thread."
                );

                snapshot_synthesizer->start();
            }

            // stops the publisher thread
            void stop() {
                running = false;
                
                // stops snapshot synthesizer thread
                snapshot_synthesizer->stop();
            }

            // 'drives' the publisher thread to send out updates to clients
            void run() noexcept {
                // PART 1: fetch the update to be published
                logger.log("%:% %() % Publisher running... \n", 
                    __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
                );

                while(running) {
                    for (const MEMarketUpdate * market_update = outgoing_md_updates->getNextRead();
                        outgoing_md_updates->size() && market_update; market_update = outgoing_md_updates->getNextRead()
                        ) {
                        
                        // almost at the last step to sending out an update from a client request
                        TTT_MEASURE(T5_MarketDataPublisher_LFQueue_read, logger);
                        
                        logger.log("%:% %() % Sending seq:% % \n", 
                            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str),
                            next_inc_seq_number, market_update->toString().c_str()
                        );

                        // write the update to the socket buffer
                        START_MEASURE(Exchange_MulticastSocket_send);
                        incremental_socket.send(&next_inc_seq_number, sizeof(next_inc_seq_number));
                        incremental_socket.send(market_update, sizeof(MEMarketUpdate));
                        END_MEASURE(Exchange_MulticastSocket_send, logger);
                        outgoing_md_updates->updateReadIndex();

                        // stop the clock! last time we do any processing on a market update
                        TTT_MEASURE(T6_MarketDataPublisher_UDP_write, logger);

                        // also send it to the synthesizer
                        MDPMarketUpdate * next_write = snapshot_md_updates.getNextWriteTo();
                        next_write->seq_number = next_inc_seq_number;
                        next_write->me_market_update = *market_update;
                        snapshot_md_updates.updateWriteIndex();

                        ++next_inc_seq_number;
                    }

                    incremental_socket.sendAndRecv();
                }

            }
    };

}