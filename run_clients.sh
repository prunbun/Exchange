#!/bin/bash

# trading_main CLIENT_ID ALGO_TYPE [trade_size_1, threshold_1, max_order_size_1, max_pos_1, max_loss_1] ... for each ticker

./cmake-build-release/trading_main  1 MAKER \
                                  100 0.6 150 300 -1000 \
                                  250 0.8 300 2000 -1100 \
                                  100 0.3 250 300 -100 &
sleep 5

./cmake-build-release/trading_main  2 MAKER \
                                  200 0.4 200 2300 -1100 \
                                  350 0.8 200 2300 -1100 \
                                  300 0.3 200 2300 -1100 &
sleep 5

./cmake-build-release/trading_main  3 TAKER \
                                  300 0.8 350 300 -300 \
                                  60 0.7 350 300 -300 \
                                  300 0.8 350 300 -300 &
sleep 5

./cmake-build-release/trading_main  4 TAKER \
                                  400 0.8 4150 4300 -1100 \
                                  450 0.9 4150 4300 -1100 \
                                  350 0.9 4450 4300 -1100 &
sleep 5

./cmake-build-release/trading_main  5 RANDOM &
sleep 5

wait