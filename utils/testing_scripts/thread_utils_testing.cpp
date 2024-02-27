#include "thread_utils.h"
#include <iostream>

// This is a function we would like the thread to execute: 
void task(int a, int b, bool should_sleep) {

    std::cout << "output = " << a + b << std::endl;

    if (should_sleep) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(5s);
    }
}

// Here is main:
int main() {
    using namespace Common;

    // create the two threads
    // note that if the thread fails to pin to a core or start a task, it will be a nullptr
    auto thread_1 = createAndStartThread(0, "worker1", task, 1, 2, false);
    auto thread_2 = createAndStartThread(1, "worker2", task, 2, 3, true);

    // join the threads back to main
    thread_1->join();
    thread_2->join(); // you will notice that main() will wait until thread_2 wakes up

    std::cout << "main() exiting..." << std::endl;
    
    return 0;
}

