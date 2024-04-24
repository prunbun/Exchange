#ifndef LOCK_FREE_QUEUE
#define LOCK_FREE_QUEUE

#include <iostream>
#include <vector>
#include <atomic>

#include "macros.h"

namespace Common {

    /*
        We want a way for different processes to communicate with each other and in such a way that multiple threads can read data concurrently.
        An application is that we can have several worker threads complete several tasks and send the results to the queue
        On the other end, a receiver can compile all the results by reading from the queue

        This queue is a subset of this functionality in that it enforces the invariant of:
        Single Producer Single Consumer (SPSC) – that is, only one thread writes to the queue and only one thread consumes from the queue.

        Importantly, this queue does not use locks or mutexes, meaning that there will be less context switches, reducing latency
    */
    template<typename T>
    class LFQUEUE final {
        private:
            std::vector<T> queue; 

            std::atomic<size_t> next_read_index = 0;
            std::atomic<size_t> next_write_index = 0;
            std::atomic<size_t> num_elements = 0;

        public:
            /* Constructors */

            LFQUEUE(size_t num_elements): queue(num_elements, T()) {};

            LFQUEUE() = delete; // Cannot instantiate the queue without passing in num_elements
            LFQUEUE(const LFQUEUE&) = delete; // Cannot copy the queue
            LFQUEUE& operator=(const LFQUEUE&) = delete; // Cannot copy using copy assignment
            LFQUEUE(const LFQUEUE&&) = delete; // Cannot use move constructor
            LFQUEUE& operator=(const LFQUEUE&&) = delete; // Cannot use move assignment



            /* LFQueue public functions */

            // returns a pointer to the next object in the queue that the user can modify
            T* getNextWriteTo() noexcept {
                return &queue[next_write_index];
            }

            // finalizes the current write object as complete and increments the queue size
            void updateWriteIndex() noexcept {
                next_write_index = (next_write_index + 1) % queue.size();
                num_elements++;
            }

            // returns a pointer to the next object that can be read
            // if the queue is empty, returns null
            const T* getNextRead() const noexcept { // <- NOTE: functions that do not modify state should be marked with 'const'
                return (next_read_index == next_write_index ? nullptr : &queue[next_read_index]);
            }

            // marks the element at 'next_read_index' as read and increments the queue
            void updateReadIndex() noexcept {
                next_read_index = (next_read_index + 1) % queue.size();

                // after we read from the queue, we 'consume' the element
                // if there are no elements left, and we just read, we have a problem
                ASSERT(num_elements != 0, "Tried to read from an empty queue!");
                num_elements--;
            }

            // returns the size of the queue
            size_t size() const noexcept {
                return num_elements.load();
            }

    };

}

#endif