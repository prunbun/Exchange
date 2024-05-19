# Exchange
In this project, I build an end-to-end financial trading exchange that includes an order-book and a client, along with surrounding infrastructure!

Importantly, this code is fully written in Modern C++ and is designed to be low-latency. <br />

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

Part 2: The Exchange
------
The second phase of the project includes implementing an application that uses our core building blocks, namely an exchange and its associated infrastructure.
This includes:
1. Publishing data to all participants
2. Receiving orders
3. Matching Engine
