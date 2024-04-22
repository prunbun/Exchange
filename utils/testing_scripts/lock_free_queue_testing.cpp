#include "../lock_free_queue.h"
#include "../thread_utils.h"

using namespace Common;

struct DummyType {
    int data[3];
};

// this will be the function that we want the 'read' thread to execute
void readThreadTask(LFQUEUE<DummyType>* queue) {
    // wait for the queue to populate
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(5s);

    // begin to read at the same rate at which the queue is being populated -> every 1 second
    std::cout << "queue_size: " << queue->size() << std::endl;
    while (queue->size()) {
        const DummyType* read_obj = queue->getNextRead();
        std::cout << "The read thread is reading an object! " 
        << read_obj->data[0] << ", " << read_obj->data[1] << ", " << read_obj->data[2] 
        << std::endl;
        queue->updateReadIndex();

        std::this_thread::sleep_for(1s);
    }

    std::cout << "read thread exiting" << std::endl;
}

int main() {
    /*
        Our test's structure will look like the following:
            1. Create a test queue object of DummyType data
            2. Start a thread that will read from the queue, while main writes to the queue
            3. Join the read thread to main before exiting to ensure that the read terminates without 
                throwing an ASSERT, indicating that read had an issue, particulary with concurrency

        NOTE: We want to make sure that the read thread sleeps for 5 seconds to allow the write thread to populate the queue
    */
    const int NUM_ELEMENTS_TO_WRITE = 50;
    LFQUEUE<DummyType> test_queue(20);

    // start the read task
    auto read_thread = createAndStartThread(-1, "read thread", readThreadTask, &test_queue);

    // start populating the queue, so the read thread can read it
    for (int i = 0; i < NUM_ELEMENTS_TO_WRITE; ++i) {
        const DummyType write_obj({i, i * 10, i * 100});
        *(test_queue.getNextWriteTo()) = write_obj; // Note that nothing is on the heap, so when the memory under the pointer is overwritten, the stack is deallocated

        std::cout << "Main created a write object: " 
        << write_obj.data[0] << ", " << write_obj.data[1] << ", " << write_obj.data[2] 
        << std::endl;
        test_queue.updateWriteIndex();

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    // Make sure the read completes before returning
    read_thread->join();

    std::cout << "Main exiting!" << std::endl;

    return 0;
}