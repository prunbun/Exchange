# Exchange
In this project, I build an end-to-end financial trading exchange in Modern C++ that includes low-latency building blocks and infrastructure to support a matching engine!! It also includes testing scripts and simple, comprehensive commentary to explain the power of C++. <br />

Credits to Sourav Ghosh for his resources on learning low-latency C++. He has great books and I would encourage anyone who is interested to pick them up.

-----

Part 1: Core Building Blocks
------
The first phase of the project includes implementing the core building blocks of low latency applications.
This includes:
1. Threads
2. Memory Pool
3. Lock-free Queue
4. Logging Framework
5. TCP Socket

### Associated Files

| File                       | Component                                                                                         |
|----------------------------|---------------------------------------------------------------------------------------------------|
| utils/macros.h             | ASSERT() and branch prediction definitions                                                        |
| utils/thread_utils.h       | creating and starting threads, including pinning threads to specific cores                        |
| utils/memory_pool.h        | allocates memory for a given template object, T, avoiding dynamic memory allocation during runtime|
| utils/lock_free_queue.h    | data structure that allows for threads to share data without using locks or mutexes               |
| utils/logger.h             | Logger class that can be used by the main thread for logging strings and format strings to a file |
| utils/tcp_socket.h         | Basic networking layer object that helps to simulate 'clients' and 'servers'                      |
| utils/tcp_server.h         | Server that highlights the 'kqueue' library to manage 'clients'                                   |
| utils/testing_scripts/     | .cpp files with tests on util components' functionality and examples of how to use them           |

Testing
-------
* Note that you can run the ``*.cpp`` tests in the ``../testing_scripts`` folder through the following commands:

```
  clang++ -std=c++20 [file_name].cpp
```
```
  ./a.out
```
<br />

-----

Part 2: The Matching Engine
------
The first major component of the exchange is called the Matching Engine. It is responsible for maintaining an internal 'order book' for passive orders and matching incoming aggresive orders.
It is also responsible for communicating with other parts of the exchange for retrieving new orders from clients and sending out updates to its order book.

### Data Stuctures and Algorithms

The algorithm using for the Matching Engine is the simple FIFO priority model. Matching orders at the same price will be executed, while differences in price for crossed orders are resolved by biasing towards the passive order's price.

The data structure design of the Matching Engine is as follows:

1. #### Order Book
    - Broadly, it is a set of two linked-lists, sorted by descending prices for bids and ascending prices for asks
    - Each price level reprents a linked-list of orders at that price level, with those of highest priority being at the head
    - All of the linked-lists used are also doubly linked to allow for easy insertion/removal and accesses to first/last list objects
3. #### Supporting Structures
    - I use a 'hashmap' to map clients <> orders id's and order id's <> order objects so we can easily access individual linked list items
    - The 'hashmaps' are actually arrays whose keys are the indices and a 'lookup' is an array access. I do this for a few reasons:
      - a) it is slightly faster than using hash functions for lookups and avoids collisions
      - b) arrays usually have more contiguous memory, helping performance
      - c) I can exploit the natural ordering of the indices and avoid overhead that true maps contain. There are weaknesses to this approach that it is possible the arrays are sparse and cannot be resized, but the exchange has set limits on what range client and order id's can take, meaning that as the exchange includes more participants, there is less 'wasted' memory.

### Associated Files

| File                                       | Component                                                                                                           |
|--------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| matching_engine/matching_engine.h          | API that other exchange components can call to start the engine and send updates to the order book                  |
| matching_engine/me_order_book.h            | core order book object, each security has a corresponding book, to which the Matching Engine forwards updates to    |
| matching_engine/matching_engine_order.h    | object that represent an order and associated data, has a helpful toString() method                                 |



