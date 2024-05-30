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

    // the synthesizer runs on a separate thread that periodically published a snapshot of the order book
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
                                const std::string &interface, const std::string &snapshot_ip, int snapshot_port
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

            // receives a market update object from the matching engine and updates the local copy of the order book
            void addToSnapshot(const MDPMarketUpdate *market_update) {
                const MEMarketUpdate &me_market_update = market_update->me_market_update;
                auto *orders = &ticker_orders.at(me_market_update.ticker_id); // copy of order book for this ticker 
                
                // now let's update our local copy depending on what the update is
                switch (me_market_update.type) {
                    case MarketUpdateType::ADD: {
                        MEMarketUpdate * order = orders->at(me_market_update.order_id);
                        ASSERT(order == nullptr, "Received:" + me_market_update.toString() + " but order already exists:" + (order ? order->toString() : ""));

                        orders->at(me_market_update.order_id) = order_pool.allocate(me_market_update);
                    }
                        break;
                    
                    case MarketUpdateType::MODIFY: {
                        MEMarketUpdate * order = orders->at(me_market_update.order_id);
                        ASSERT(order != nullptr, "Received:" + me_market_update.toString() + " but order does not exist.");
                        ASSERT(order->order_id == me_market_update.order_id, "received conflicting order id's");
                        ASSERT(order->side == me_market_update.side, "received conflicting sides");

                        order->qty = me_market_update.qty;
                        order->price = me_market_update.price;
                    }
                        break;

                    case MarketUpdateType::CANCEL: {
                        MEMarketUpdate * order = orders->at(me_market_update.order_id);
                        ASSERT(order != nullptr, "Received:" + me_market_update.toString() + " but order doesn't exist. ");
                        ASSERT(order->order_id == me_market_update.order_id, "received conflicting order id's");
                        ASSERT(order->side == me_market_update.side, "received conflicting sides");

                        order_pool.deallocate(order);
                        orders->at(me_market_update.order_id) = nullptr;
                    }
                        break;

                    case MarketUpdateType::SNAPSHOT_START:
                    case MarketUpdateType::CLEAR:
                    case MarketUpdateType::SNAPSHOT_END:
                    case MarketUpdateType::TRADE:
                    case MarketUpdateType::INVALID:
                        break;
                }

                // let's record this sequence number so we make sure we preserve ordering
                ASSERT(market_update->seq_number == last_inc_seq_num + 1, "Expected incremental seq numbers to increase");
                last_inc_seq_num = market_update->seq_number;
            }

            // we have START_MARKET_UPDATE and END_MARKET_UPDATE as sentinels to indicate the start and end of a snapshot
            // for each ticker, respectively, we will send out a CLEAR update followed by ADD for each order
            // note that this is a strawman approach and we can optimize by having additional struct info fields
            void publishSnapshot() {
                size_t snapshot_size = 0;

                // start sentinel
                const MDPMarketUpdate start_market_update{
                    snapshot_size++, 
                    {MarketUpdateType::SNAPSHOT_START, last_inc_seq_num}
                };
                logger.log("%:% %() % START SENTINEL % \n",
                    __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string),
                    start_market_update.toString()
                );
                snapshot_socket.send(&start_market_update, sizeof(MDPMarketUpdate));

                // now, for each ticker, we send a clear update, then send the orders for that ticker
                for(size_t ticker_i = 0; ticker_i < ticker_orders.size(); ++ticker_i) {
                    // grab the orders for that ticker
                    const auto &orders = ticker_orders.at(ticker_i);

                    // first send a CLEAR sentinel
                    MEMarketUpdate me_market_update;
                    me_market_update.type = MarketUpdateType::CLEAR;
                    me_market_update.ticker_id = ticker_i;

                    const MDPMarketUpdate clear_market_update{snapshot_size++, me_market_update};
                    logger.log("%:% %() % CLEAR SENTINEL % \n",
                        __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string),
                        clear_market_update.toString()
                    );
                    snapshot_socket.send(&clear_market_update, sizeof(MDPMarketUpdate));
                    
                    // for each potential order id, if it exists, we will publish it in our update
                    for (const auto order: orders) {
                        if (order) {
                            const MDPMarketUpdate market_update{snapshot_size++, *order};
                            logger.log("%:% %() % % \n",
                                __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string),
                                market_update.toString()
                            );
                            snapshot_socket.send(&market_update, sizeof(MDPMarketUpdate));
                            snapshot_socket.sendAndRecv();
                        }
                    }
                }

                // end sentinel
                const MDPMarketUpdate end_market_update{
                    snapshot_size++, 
                    {MarketUpdateType::SNAPSHOT_END, last_inc_seq_num}
                };
                logger.log("%:% %() % END SENTINEL % \n",
                    __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string),
                    end_market_update.toString()
                );
                snapshot_socket.send(&end_market_update, sizeof(MDPMarketUpdate));
                snapshot_socket.sendAndRecv();

                logger.log("%:% %() % Published snapshot of % orders. \n",
                    __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string),
                    snapshot_size - 1
                );
            }

            void run() {
                logger.log("%:% %() % Synthesizer running... \n",
                    __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string)
                );

                while(running) {
                    // iterate through all messages on the LFQ from the publisher
                    for (const MDPMarketUpdate * market_update = snapshot_md_updates->getNextRead();
                        snapshot_md_updates->size() && market_update; 
                        market_update = snapshot_md_updates->getNextRead()
                    ) {
                        logger.log("%:% %() % Run is processing % \n",
                            __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_string),
                            market_update->toString().c_str()
                        );

                        addToSnapshot(market_update);
                        snapshot_md_updates->updateReadIndex();
                    }

                    // if it has been a while since the last update, let's publish another update
                    if(getCurrentNanos() - last_snapshot_time > 60 * NANOS_TO_SECONDS) {
                        last_snapshot_time = getCurrentNanos();
                        publishSnapshot();
                    }
                }
            }
    };
}