# Exchange
In this project, I build an end-to-end financial trading exchange that includes an order-book and a client, along with surrounding infrastructure! It also includes testing scripts and simple, comprehensive commentary to explain the power of C++.

Importantly, this code is fully written in Modern C++ and is designed to be low-latency. <br />

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

The data structure design of the Matching Engine is as follows:
1. Order Book:
   - Represented as two linked lists of prices.



