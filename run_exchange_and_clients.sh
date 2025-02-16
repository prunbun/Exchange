#!/bin/bash

bash ./build.sh

date

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Clearing Log Files..."
echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
rm -f *.log

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Starting Exchange..."
echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
./cmake-build-release/exchange_main 2>&1 &
sleep 10

bash ./run_clients.sh

sleep 45s

echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
echo "Stopping Exchange..."
echo "---------------------------------------------------------------------------------------------------------------------------------------------------------"
pkill -2 exchange

sleep 10s

wait
date