# A Financial Exchange and Trading Client in Modern C++

## Contents
- [Introduction](./README.md#introduction)
- [Part 1: Core Building Blocks](./README.md#core-building-blocks)
- [Part 2: The Matching Engine](./README.md#the-matching-engine)
- [Part 3: The Order Gateway](./README.md#the-order-gateway)
- [Part 4: The Market Publisher](./README.md#the-market-publisher)
- [Part 5: Client Design Breakdown](./README.md#client-design-breakdown)
- [Part 6: Client Ecosystem Example Walkthrough](./README.md#ecosystem-example-walkthrough)
- [Running the Exchange](./README.md#running-the-exchange)
- [Tradeoffs and Future Items](./README.md#tradeoffs-and-future-items)

## Introduction

In this project, I build an end-to-end financial exchange and trading client in Modern C++. 
- First, I build the low-latency building blocks of the project, including low-latency building blocks like custom wrappers of sockets, concurrency-safe data structures, and logging tools and more.
- Next, I use these abstractions to build out the two core components of this project, the matching and trading engines!
- The repo also includes testing scripts and simple, comprehensive code comments to make the code very reader friendly!

#### Recommended Prerequisites
To have the best experience when reviewing the code, I would recommend for the user to be familiar with the following topics:
> - Modern C++ (Memory Management, Pointers/Refereces, Templates, Object-Oriented Programming)
> - Networking
> - Basic Compiler Code Optimization Techniques
> - Concurrency
> - Data Structures and Algorithms

#### Credits and Additional Notes
CMake and Ninja are used as build tools for this project. Code is written to be compiled on a MacOS machine.

Credits to Sourav Ghosh for his resources on learning low-latency C++ and the design of this ecosystem. He has great books and I would encourage anyone who is interested in learning about low-latency systems to pick them up!

## Core Building Blocks

The first phase of the project includes implementing the core building blocks of low latency applications.
This includes:
1. Threads
2. Memory Pool
3. Lock-free Queue
4. Logging Framework
5. TCP Socket

### Highlighted Files

| File                       | Description                                                                                       |
|----------------------------|---------------------------------------------------------------------------------------------------|
| utils/macros.h             | ASSERT() and branch prediction definitions                                                        |
| utils/thread_utils.h       | creating and starting threads, including pinning threads to specific cores                        |
| utils/memory_pool.h        | allocates memory for a given template object, T, avoiding dynamic memory allocation during runtime|
| utils/lock_free_queue.h    | data structure that allows for threads to share data without using locks or mutexes               |
| utils/logger.h             | Logger class that can be used by the main thread for logging strings and format strings to a file |
| utils/tcp_socket.h         | Basic networking layer object that helps to simulate 'clients' and 'servers'                      |
| utils/tcp_server.h         | Server that highlights the 'kqueue' library to manage 'clients'                                   |
| utils/testing_scripts/     | .cpp files with tests on util components' functionality and examples of how to use them           |

> Note that all of these components have corresponding correctness tests in `utils/testing_scripts/` which also serve as examples on how to use the components in isolation.
<br />

## The Matching Engine

The first major component of the exchange is called the Matching Engine. It is responsible for maintaining an internal 'order book' for passive orders and matching incoming aggresive orders.
It is also responsible for communicating with other parts of the exchange for retrieving new orders from clients and sending out updates to its order book.

### Data Stuctures and Algorithms

The algorithm using for the Matching Engine is the simple FIFO priority model. Matching orders at the same price will be executed, while differences in price for crossed orders are resolved by biasing towards the passive order's price.

The data structure design of the Matching Engine is as follows:

1. #### Order Book
    - Broadly, it is a set of two linked-lists, sorted by descending prices for bids and ascending prices for asks
    - Each price level reprents a linked-list of orders at that price level, with those of highest priority being at the head
    - All of the linked-lists used are also doubly linked to allow for easy insertion/removal and accesses to first/last list objects
2. #### Supporting Structures
    - I use a 'hashmap' to map clients <> orders id's and order id's <> order objects so we can easily access individual linked list items
    - The 'hashmaps' are actually arrays whose keys are the indices and a 'lookup' is an array access. I do this for a few reasons:
      - a) it is slightly faster than using hash functions for lookups and avoids collisions
      - b) arrays usually have more contiguous memory, helping performance
      - c) I can exploit the natural ordering of the indices and avoid overhead that true maps contain. There are weaknesses to this approach that it is possible the arrays are sparse and cannot be resized, but the exchange has set limits on what range client and order id's can take, meaning that as the exchange includes more participants, there is less 'wasted' memory.

### Highlighted Files

| File                                       | Description                                                                                                         |
|--------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| matching_engine/matching_engine.h          | API that other exchange components can call to start the engine and send updates to the order book                  |
| matching_engine/me_order_book.h            | core order book object, each security has a corresponding book, to which the Matching Engine forwards updates to    |
| matching_engine/matching_engine_order.h    | object that represent an order and associated data, has a helpful toString() method                                 |
<br />

## The Order Gateway

The second major component of the exchange is called the Order Gateway. It is responsible for accepting connections from market participants, receiving their orders, and providing them with updates on their orders' status.

There are a few important functions that this part performs
1. Protocol for Communication
    - One that is internal with the Matching Engine and another that is public with market participants that includes sequence id's for ensuring proper communication
    - Importantly, even though the Order Server operates using TCP, it includes sequence numbers for maintaining the correctness of the application layer
3. FIFO Sequencing 
    - Sorts using the time at which the server's socket receives orders from participants to ensure fairness
4. Privacy
    - The Order Server communicates with participants directly, ensuring that only they can receive sensitive data regarding the order
    - Using UDP would be a privacy violation

### Highlighted Files

| File                                       | Description                                                                                                         |
|--------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| order_gateway/order_server.h               | main file, includes definitions of callbacks regarding what the sockets and server should do upon receiving an order|
| order_gateway/fifo_sequencer.h             | sorts and sends orders to the matching engine after receiving them from the gateway                                 |
<br />

## The Market Publisher

The final major component of the exchange is called the Market Publisher. It is responsible for providing updates to the order book to all market partipants via UDP. It has two threads, one sends out only deltas in the order book while the other periodically sends out a full snapshot of the order book.

### Highlighted Files

| File                                       | Description                                                                                                                            |
|--------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------|
| market_publisher/market_data_publisher.h   | As it receives updates from the matching engine, the thead sends them out using the .sendAndRecv() method defined for the UDP socket   |
| market_publisher/snapshot_synthesizer.h    | Accumulates a collection of orders that represent a lightweight, local copy of the order book, and periodically sends out the snapshot |
<br />

## Client Design Breakdown

The client design largely complements that of the exchange. Here is a brief visual on the corresponding components between the exchange and trading client.

| Exchange Component                                      | Trading Component                           |
|---------------------------------------------------------|---------------------------------------------|
| Order Server (manages clients)                          | Order Gateway (places orders)               |
| Matching Engine (crosses orders)                        | Trading Engine ('brain' of the client)      |
| Market Publisher (sends out data to market subscribers) | Market Data Consumer (listens for updates)  |

<br />

Below, the first subsection describes the similarities and differences with the exchange. The second subsection outlines how a trader can implement a new strategy to deploy on the exchange.

### Comparing with the Exchange

- Similarities
    - The order book structure and operations are nearly identical to the exchange. The client typically does not have access to the real-time order book used by the exchange. As a result, the client's objective is to recreate a copy of the exchange's order book as closely and as fast as possible to have a comprehensive and competitive view of the market.
    - The order gateway performs largely the same send and receive operations, with the only difference being that the client's gateway is signficantly simpler since it only has to maintain 1 TCP connection as opposed to the exchange.
- Differences
    - The client's market data consumer must now recognize if it has missed any updates on the UDP multicast. If so, it must now subscribe to the snapshot stream to recreate its order books from scratch. As one might expect, this is expensive, but necessary (for most client strategies).
    - The trading engine now computes 'features' from the 'signals' provided by the market, which may include metrics such as projected market price, trade-volume ratios etc.
    - The trade engine doesn't directly place trades, instead, it depends on the logic provided from a quantitative trading strategy file, which instead manages the PositionKeeper, RiskManager, and OrderManager classes
 
### Implementing a New Trading Strategy

Whenever an order book change, trade update, or exchange receipt event occurs, the trade engine notifies the respective trading strategy file, which can 'react' by updating its own metrics and/or place trades accordingly. 

More specifically, the file should implement the following 3 methods
1. `onOrderBookUpdate` - called when the trade engine detects a change for a particular instrument's best price/qty
2. `onTradeUpdate` - called when the trade engine detects a trade between anonymous market participants has occurred
3. `onOrderUpdate` - called when the order gateway has received a status update from one of a client's live orders

> Examples for two strategies can be found here:
> - Market Making Algorithm: `trading/strategy/market_maker.h` , `trading/strategy/market_maker.cpp`
> - Liquidity Taking Algorithm: `trading/strategy/liquidity_taker.h` , `trading/strategy/liquidity_taker.cpp`

Next, an enum can be added in `/utils/orderinfo_types.h` and updated in the `trade_engine.cpp` constructor to initialize the strategy (in the future, it should be made into a general template)
Finally, examples for running clients with a particular strategy can be found here: `./run_clients.sh`
> NOTE: If the enum RANDOM is used, then it will use the default random strategy outlined in `trading_main.cpp`


<br />

## Client Ecosystem Example Walkthrough

Here, I describe the flow of data through the client ecosystem to help readers understand how to navigate the codebase!

#### 1. Client Gets A Market Update - `/trading/market_data/market_data_consumer.cpp`

- The client is subscribed to the UDP incremental updates from the exchange.
- We sequentially read the data from the receiving socket's buffer.
- After checking the message's validity, we send it to the trading engine.

#### 2. The Trade Engine 'Reacts' - `/trading/strategy/trade_engine.cpp`

- It forwards the update to the MarketOrderBook object for the corresponding instrument.
- If the update indicates a change in the order book or a `TRADE` occured, the PositionKeeper and FeatureEngine are notified to update the PnL and calculated features, respectively.
- Based on changes to the order book and features, the custom client quant trading strategy will 'react'.

#### 3. The Trade Engine Places Trades - `/trading/strategy/order_manager.h`

- The OrderManager is invoked from the client's strategy files to place trades.
- The OrderManager checks the risk the client is taking on before forwarding the trade requests to the client order gateway.

#### 4. The Order Gateway -  `/trading/order_gw/order_gateway.cpp`

- The order gateway writes the order object to its local buffer ready for its next send.
- It sends data from the local buffer using TCP to the exchange, expecting status 'receipts' from the exchange.
- It also sequentially reads data from its receive buffer, checks its validity, and forwards the responses from the exchange to the trading engine.

#### 5. The Trade Engine Processes Receipts - `/trading/strategy/trade_engine.cpp`

- The trade engine forwards the exchange response to the client strategy engine to recalculate its PnL.
- The custom trading strategy can also place trades here, if the trading strategy finds these receipts useful for its algorithms.
<br />

## Running the Exchange

The two core files that make up this ecosystem are 
- `exchange/exchange_main.cpp`
- `trading/trading_main.cpp`
<br />

I used CMake and Ninja to make the build/run process easier. `build.h` in the main directory will help to compile both of these files into executables in directory at `./cmake_build_release/`.
Below, I have outlined commands to run the application. Once the process has been terminated, results can be seen in corresponding `.log` files!

#### Exchange and Single Client
```
  ./build.sh
```
```
  ./cmake-build-release/exchange_main
```
```
  ./cmake-build-release/trading_main [CLIENT_ID] [TRADING_STRATEGY_NAME]
```
<br />

#### Several Clients at Once
As an example of how to run several clients at once, please see the see the script called `./run_clients.sh`

Each client must have a client id (integer) specified as well as an enum for the trading strategy it uses. Please see the seciton on **Client Design Breakdown** for other steps necessary to implement a custom strategy. For each of the tickers possible to trade on the exchange, one can specify characteristics like `max_trade_qty`, `max_position`, and `max_loss`, etc. Please see `trading_main.cpp` for more specifics.

#### Example Ecosystem Run
To see an example of the exchange running with 5 clients, please run the script `./run_exchange_and_clients.sh`. For the exchange as well as each client, there will be log files generated for each component which can be inspected for state. You should be able to trace specific trades throughout the ecosystem by going through the log files.

All timing information can be found in the logs, including checkpoints as data flows through the exchange and as events occur in real-time, down to the nanosecond-granular timestamp.

#### Tradeoffs and Future Items

There are a few areas that could be improved in the future for next steps:
<br />
- Looking into using C++ STL map instead of linked-lists for the order book
- Using unordered_maps as true hashmaps instead of arrays
- Having more security around valid requests to the exchange and offset errors in the receiving buffers
- Addressing scalability with additional clients with adding more layers or splitting into several exchanges
- Making trading strategy integrations more general using base classes
- Visualizing the latencies using jupyter notebooks on the produced logs
- Writing more unit tests for specific strategies and order book snapshot reconstruction edge-cases
- Optimize logging functionality to help terminate processes quicker at the end of a program.
<br />
- Developing a frontend to visualize the order book in real-time
