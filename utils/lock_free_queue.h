#ifndef LOCK_FREE_QUEUE
#define LOCK_FREE_QUEUE

#include <iostream>
#include <vector>
#include <atomic>

namespace Common {

    /*
        We want a way for different processes to communicate with each other
    */
    template<typename T>
    class LFQUEUE final {
        private:
            std::vector<T> queue; 

            std::atomic<size_t> next_read_index = 0;
            std::atomic<size_t> next_write_index = 0;
            std::atomic<size_t> num_elements = 0;

        public:

    };

}

#endif