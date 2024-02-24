#include "memory_pool.h"

// Object type that a user would want to store into the MemPool
struct ExampleType {
    int data[2];
};

int main() {

    using namespace Common;

    /*
        Note that 

            MemPool<ExampleType>* new_mem_pool = new MemPool(50);

        could work, but since the MemPool has a fixed size, 
        it is optimal to have it on the stack in this case
    */
    const int new_mem_pool_size = 50;
    MemPool<ExampleType> new_mem_pool(new_mem_pool_size);

    for (int i = 0; i < new_mem_pool_size + 1; i++) {
        // remember that delete is not required as new_mem_pool will be deallocated after main() exits
        ExampleType* allocated_data = new_mem_pool.allocate(ExampleType{{1, 2}}); 
        
        std::cout << "the custom object is allocated at location: " << allocated_data
            << " who's first element is: " << allocated_data->data[0]
            << " and will now be deallocated" << std::endl;

        new_mem_pool.deallocate(allocated_data); // commenting this out will yield: ASSERT: storage is full!
    }
    
    return 0;
}