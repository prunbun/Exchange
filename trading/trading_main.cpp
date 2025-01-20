#include <csignal>

#include "strategy/trade_engine.h"
#include "order_gw/order_gateway.h"
#include "market_data/market_data_consumer.h"

#include "utils/logger.h"

Common::Logger *logger = nullptr;
Trading::TradeEngine *trade_engine = nullptr;
Trading::MarketDataConsumer *market_data_consumer = nullptr;
Trading::OrderGateway *order_gateway = nullptr;

/*
    main() accepts arguments in the following form
    trading_main CLIENT_ID ALGO_TYPE [trade_size_1, threshold_1, max_order_size_1] ... for each ticker
*/
int main(int argc, char **argv) {

    // ----------------
    // PARSE INPUTS
    // ----------------
    // parse client id and algo type
    const Common::ClientId client_id = atoi(argv[1]);
    srand(client_id); // set rng seed
    const AlgoType algo_type = stringToAlgoType(argv[2]);

    // parse trading strategy configs
    TradeEngineConfigHashmap ticker_configs_hashmap;
    size_t next_ticker_id = 0;
    for (int i = 3; i < argc; i += 5, ++next_ticker_id) {

        ticker_configs_hashmap.at(next_ticker_id) = {
                                                        // TradeEngineConfig    
                                                        static_cast<Qty>(std::atoi(argv[i])), // trade_size
                                                        std::atof(argv[i + 1]), // feature threshold

                                                        {
                                                            // RiskConfig
                                                            static_cast<Qty>(std::atoi(argv[i + 2])), // max order size
                                                            static_cast<Qty>(std::atoi(argv[i + 3])), // max position
                                                            std::atof(argv[i + 4]) // max loss
                                                        }
                                                    };
    }

    // initialize support structures
    logger =  new Common::Logger("trading_main" + std::to_string(client_id) + ".log");
    const int sleep_time = 20 * 1000;

    Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);


    // ----------------
    // START ALL CLIENT COMPONENTS
    // ----------------
    std::string time_str;

    // start trading engine
    logger->log("%:% %() % Starting Trade Engine... \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
    );
    trade_engine = new Trading::TradeEngine(client_id, algo_type, ticker_configs_hashmap, &client_requests, &client_responses, &market_updates);
    trade_engine->start();

    // starting (client) order gateway
    const std::string order_gateway_ip = "127.0.0.1";
    const std::string order_gateway_interface = "lo";
    const int order_gateway_port = 12345;
    logger->log("%:% %() % Starting Order Gateway... \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
    );
    order_gateway = new Trading::OrderGateway(client_id, &client_requests, &client_responses, order_gateway_ip, order_gateway_interface, order_gateway_port);
    order_gateway->start();

    // starting market data consumer
    const std::string mkt_data_interface = "lo";
    const std::string snapshot_ip = "233.252.14.1";
    const int snapshot_port = 20000;
    const std::string incremental_ip = "233.252.14.3";
    const int incremental_port = 20001;
    logger->log("%:% %() % Starting Market Data Consumer... \n",
        __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
    );
    market_data_consumer = new Trading::MarketDataConsumer(client_id, &market_updates, mkt_data_interface, snapshot_ip, snapshot_port, incremental_ip, incremental_port);
    market_data_consumer->start();


    // ----------------
    // SLEEP FOR WARM UP
    // ----------------
    usleep(10 * 1000 * 1000);
    trade_engine->initLastEventTime();

    // ----------------
    // LAUNCH STRATEGIES
    // ----------------
    



}