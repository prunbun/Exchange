# Exchange
In this project, I build an end-to-end financial trading exchange that includes an order-book and a client, along with surrounding infrastructure!

Importantly, this code is fully written in Modern C++ and is designed to be low-latency. <br />
Credits for study material: [Sourav Ghosh](https://www.packtpub.com/product/building-low-latency-applications-with-c/9781837639359)

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

Testing
-------
* Note that you can run the ``*.cpp`` tests in the ``../testing_scripts`` folder through the following commands:

```
  clang++ -std=c++20 [file_name].cpp
```
```
  ./a.out
```

