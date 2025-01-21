#!/bin/bash

# trading_main CLIENT_ID ALGO_TYPE [trade_size_1, threshold_1, max_order_size_1, max_pos_1, max_loss_1] ... for each ticker

# ./cmake-build-release/trading_main  1 MAKER \
#                                   100 0.6 150 300 -100 \
#                                   60 0.6 150 300 -100 \
#                                   150 0.5 250 600 -100 \
#                                   200 0.4 500 3000 -100 \
#                                   1000 0.9 5000 4000 -100 \
#                                   300 0.8 1500 3000 -100 \
#                                   50 0.7 150 300 -100 \
#                                   100 0.3 250 300 -100 &
# sleep 2

# ./cmake-build-release/trading_main  2 TAKER \
#                                   300 0.8 350 300 -300 \
#                                   60 0.7 350 300 -300 \
#                                   350 0.5 250 600 -300 \
#                                   200 0.6 500 3000 -300 \
#                                   3000 0.5 5000 4000 -300 \
#                                   300 0.7 3500 3000 -300 \
#                                   50 0.3 350 300 -300 \
#                                   300 0.8 350 300 -300 &

./cmake-build-release/trading_main  3 RANDOM &
sleep 5

wait