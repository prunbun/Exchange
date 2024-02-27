# Exchange
In this project, I build an end-to-end financial trading exchange that includes an order-book and a client, along with surrounding infrastructure!

Importantly, this code is fully written in Modern C++ and is designed to be low-latency.

Part 1: Supporting Infra
------
The first phase of the project includes implementing the core building blocks of low latency applications.

| File                       | Component                                                                 |
|----------------------------|---------------------------------------------------------------------------|
| utils/thread_utils.h       | creating and starting threads, including pinning threads to specific cores
