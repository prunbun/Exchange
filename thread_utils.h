#ifndef THREEAD_UTILS_H
#define THREEAD_UTILS_H

#include <iostream>
#include <cstring>
#include <atomic> // access to thread-safe variables
#include <unistd.h> // access to functions such as sleep()
#include <sys/syscall.h> // access to CPU flags
#include <thread_utils_pin_cores.h> // helper functions to allow access to macOS kernel API

using std::string;
using std::atomic;
using std::cerr;
using std::forward;
using std::thread;

namespace ThreadUtils {

    inline bool pinThreadToCore(int core_id) noexcept {
        cpu_set_t cpu_set; // bits representing available cores
        CPU_ZERO(&cpu_set); // clear all pins on cores
        CPU_SET(core_id, &cpu_set); // add a pin on a particular core

        // pin this thread on the specified core, and return 0 if it succeeded
        return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set) == 0);
    }

    template<typename T, typename... Params>
    inline auto createAndStartThread(
        int core_id,
        const string &thread_name,
        T &&func_task_for_thread,
        Params &&... func_task_args
    ) noexcept {
        
        // thread-safe variables to ensure that the thread was 'pinned' and given a task
        atomic<bool> pin_failed{false};
        atomic<bool> running{false};

        /* 
            Lambda function used by thread constructor on startup
            It should:
            1. Pin itself to a core to help CPU avoid context switching
            2. Begin the task specified by the func_task
        */
        auto thread_init = [&] {
            // if the thread failed to pin to a valid core, return and indicate that the thread failed
            if (core_id >= 0 && !pinThreadToCore(core_id)) {
                cerr << "Failed to set core affinity for " << name << " " << pthread_self() 
                << " to core " << core_id << endl;

                failed = true;
                return;
            }

            // The thread was pinned successfully, now, we can assign the task to the thread
            running = true;
            forward<T>(func_task_for_thread) ((forward<Params>(func_task_args))...); 
        }

        
        // create the new thread
        auto new_thread = new thread(thread_init);

        // The thread is still running its init, so not created yet
        while (!running && !pin_failed) {
            using std::literals::chrono_literals;
            std::this_thread::sleep_for(1s);
        }

        // thread could not init properly
        if(pin_failed) {
            new_thread->join();
            delete new_thread;
            new_thread = nullptr;
        }

        return new_thread;
    }
}

/*
    Sample code that would use this thread util
    ---


    This is a function we would like the thread to execute: 
    void task(int a, int b, bool should_sleep) {

        cout << "output = " << a + b << endl;

        if (should_sleep) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(5s);
        }
    }

    Here is main:
    int main() {

        // create the two threads
        // note that if the thread fails to pin to a core or start a task, it will be a nullptr
        auto thread_1 = createAndStartThread(0, "worker1", task, 1, 2, false);
        auto thread_2 = createAndStartThread(1, "worker2", task, 2, 3, true);

        // join the threads back to main
        thread_1->join();
        thread_2->join(); // you will notice that main() will wait until thread_2 wakes up

        cout << "main() exiting..." << endl;
        
        return 0;
    }

*/

#endif