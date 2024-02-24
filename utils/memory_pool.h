#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <vector>
#include <string>
#include <cstdint>

#include "macros.h"

namespace Common {

    /* 
        we want to create a mempool such that we are not dynamically allocating memory during the program
        dynamically allocated memory has poor performance in low-latency C++ because of fragmentation, overhead, and multithreading locks
    */
    template<typename T>
    class MemPool final {
        private: 
            struct ObjectBlock {
                T memory_block; // note that the order the members are in matters!
                bool is_free;
            };

            std::vector<ObjectBlock> storage; 
            size_t next_available_space = 0;

            // updates the next index that an allocated object should be placed in the MemPool
            void updateNextAvailableSpace() noexcept {
                const size_t current_next_avail_space = next_available_space;

                while (!storage[next_available_space].is_free) {
                    ++next_available_space;

                    // if we reached the end of the storage, loop back around to the front
                    if (UNLIKELY(next_available_space == storage.size())) {
                        next_available_space = 0;
                    }

                    // if we ever reach the same index as we had before, now we know storage is full
                    if (UNLIKELY(next_available_space == current_next_avail_space)) {
                        ASSERT(next_available_space != current_next_avail_space, "storage is full!");
                        break;
                    }
                }
            }

        public:
            // note that explicit keyword stops implicit conversions of the class MemPool (using a size_t where MemPool obj is expected) 
            explicit MemPool(std::size_t num_elements): storage(num_elements, {T(), true}) {}

            // these need to be deleted so we can control how the MemPool class is used by the programmer and by the compiler
            MemPool() = delete; // default constructor
            MemPool(const MemPool&) = delete; // copy constructor
            MemPool(const MemPool&&) = delete; // move constructor
            MemPool& operator=(const MemPool&) = delete; // copy assignment constructor, note: returns a ref to the l-value
            MemPool& operator=(const MemPool&&) = delete; // move assignment constructor

            // way for user to store into the MemPool
            template<typename... T_Params>
            T* allocate(T_Params... args) noexcept {
                // retrieve the free memory slot
                // note that the ASSERT does not throw an exception, only prints to std::cerr, so the 'noexcept' keyword holds
                auto next_free_obj_block = &(storage[next_available_space]);
                ASSERT(next_free_obj_block->is_free, "mem pool block fetched during allocate() must be available!");

                /*
                    Three important points here:

                    1. We fetch the pre-allocated memory from our MemPool to prevent dynamic memory allocation
                    2. We pass in the existing pointer to allocated memory to invoke the operator overloading of 'new' that stops allocation
                    3. args are directly used here, so they don't need to be forwarded, 
                        if they were passed into another function then they would have to be
                */
                T* obj_memory_addr = &(next_free_obj_block->memory_block);
                obj_memory_addr = new(obj_memory_addr) T(args...);
                next_free_obj_block->is_free = false;

                updateNextAvailableSpace();

                return obj_memory_addr;
            }

            // way for user to remove from the MemPool
            void deallocate(T* obj_to_delete) noexcept {
                /*
                    Explaining a few design choices here:
                    
                    1. we made sure that the first element of our struct was the T object, so we can use this to 'lookup' the index

                    2. note that in c++ pointer subtraction yields the number of elements that exist in between the pointers
                */
                auto delete_obj_mem_block_pointer = reinterpret_cast<const ObjectBlock*>(obj_to_delete);
                int index_of_del_obj_block = delete_obj_mem_block_pointer - &storage[0];
                ASSERT(index_of_del_obj_block >= 0 && static_cast<size_t>(index_of_del_obj_block) < storage.size(),
                        "object memory location was not within this MemPool!");

                storage[index_of_del_obj_block].is_free = true;
            }

    };
}

#endif