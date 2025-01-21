#include "risk_manager.h"
#include "order_manager.h"

Trading::RiskManager::RiskManager(Common::Logger *logger_param, const PositionKeeper *position_keeper_param, 
                                    const TradeEngineConfigHashmap &ticker_config_param):
                                    logger(logger_param)
{

    for (TickerId i = 0; i < ME_MAX_TICKERS; ++i) {
        ticker_risk_hashmap.at(i).position_info = position_keeper_param->getPositionInfo(i);
        ticker_risk_hashmap.at(i).risk_cfg = ticker_config_param[i].risk_cfg;
    }

}