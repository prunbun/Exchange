#include <csignal>

#include "../matching_engine.h"
#include "../exchange_limits.h"

Common::Logger *logger = nullptr;
Exchange::MatchingEngine *matching_engine = nullptr;

void signal_handler(int) {
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete logger;
    logger = nullptr;

    delete matching_engine;
    matching_engine = nullptr;

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

    std::string time_string;
    logger->log("%:% %() % Starting Matching Engine... \n",
        __FILE__, __LINE__, __FUNCTION__,
        Common::getCurrentTimeStr(&time_string)
    );

    matching_engine = new Exchange::MatchingEngine(&client_requests, &client_responses, &market_updates); 
    matching_engine->start();

    // dummy code that makes this sleep forever, will change this during testing
    while(true) {
        logger->log("%:% %() % Sleeping for now... \n",
            __FILE__, __LINE__, __FUNCTION__,
            Common::getCurrentTimeStr(&time_string)
        );
        usleep(sleep_time * 1000);
    }
}
