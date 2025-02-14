#include <csignal>

#include "matching_engine/matching_engine.h"
#include "market_publisher/market_data_publisher.h"
#include "order_gateway/order_server.h"
#include "../utils/exchange_limits.h"

Common::Logger *logger = nullptr;
Exchange::MatchingEngine *matching_engine = nullptr;
Exchange::MarketDataPublisher *market_data_publisher = nullptr;
Exchange::OrderServer *order_server = nullptr;

void signal_handler(int) {
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete logger;
    logger = nullptr;

    delete matching_engine;
    matching_engine = nullptr;

    delete market_data_publisher;
    market_data_publisher = nullptr;

    delete order_server;
    order_server = nullptr;

    std::this_thread::sleep_for(10s);

    exit(EXIT_SUCCESS);
}

int main() {
    logger = new Common::Logger("exchange_main.log");

    // whenever we do ctrl-c etc., it sends a signal to the program, like SIGINT
    // programs can define a handler of what to do if they receive such a signal
    std::signal(SIGINT, signal_handler);

    const int sleep_time = 100 * 1000;

    Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

    std::string time_str;
    logger->log("%:% %() % Starting Matching Engine... \n",
        __FILE__, __LINE__, __FUNCTION__,
        Common::getCurrentTimeStr(&time_str)
    );

    // starting the matching engine
    matching_engine = new Exchange::MatchingEngine(&client_requests, &client_responses, &market_updates); 
    matching_engine->start();

    // starting the publisher server
    const std::string mkt_publisher_interface = "lo";
    const std::string snapshot_publisher_ip = "233.252.14.1", inc_publisher_ip = "233.252.14.3";
    const int snapshot_publisher_port = 20000, inc_publisher_port = 20001;

    logger->log("%:% %() % Starting Publisher... \n",
        __FILE__, __LINE__, __FUNCTION__,
        Common::getCurrentTimeStr(&time_str)
    );
    market_data_publisher = new Exchange::MarketDataPublisher(
        &market_updates, mkt_publisher_interface,
        snapshot_publisher_ip, snapshot_publisher_port,
        inc_publisher_ip, inc_publisher_port
    );
    market_data_publisher->start();

    // starting the order server
    const std::string order_gateway_interface = "lo";
    const int order_gateway_port = 12345;

    logger->log("%:% %() % Starting Gateway... \n",
        __FILE__, __LINE__, __FUNCTION__,
        Common::getCurrentTimeStr(&time_str)
    );
    order_server = new Exchange::OrderServer(
        &client_requests, &client_responses, 
        order_gateway_interface, order_gateway_port
    );
    order_server->start();

    // ---------
    // making this code run until it is explicitly killed by the user
    while (true) {
        logger->log("%:% %() % Keeping exchange alive... \n",
            __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str)
        );
        usleep(sleep_time * 1000);
    }
}
