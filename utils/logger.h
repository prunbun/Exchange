#pragma once

#include <string>
#include <fstream> // library for allowing writing to output files
#include "logtype.h"
#include <iostream>

namespace Common {

    /*
        Logger object that will be used to log metrics and performance messages during the runtime
    */
    class Logger final { // note the use of final keyword which indicates this cannot be inherited

        private:
            const std::string filename;
            std::ofstream file; // stream that writes to a file
            LFQUEUE<LogElement> queue;
            std::atomic<bool> running = true; // switch for keeping the logger thread active
            std::thread *logger_thread = nullptr;

        public:

            void flushQueue() noexcept {
                while (running) {
                    // next will be a pointer to the next queue object
                    for (const LogElement* next = queue.getNextRead(); (queue.size() > 0) && (next != nullptr); next = queue.getNextRead() ) {

                        // write to the file
                        switch (next->type) {
                            case LogType::CHAR:
                                file << next->u.c; break;
                            case LogType::INTEGER:
                                file << next->u.i; break;
                            case LogType::LONG_INTEGER:
                                file << next->u.l; break;
                            case LogType::LONG_LONG_INTEGER:
                                file << next->u.ll; break;
                            case LogType::UNSIGNED_INTEGER:
                                file << next->u.u; break;
                            case LogType::UNSIGNED_LONG_INTEGER:
                                file << next->u.ul; break;
                            case LogType::UNSIGNED_LONG_LONG_INTEGER:
                                file << next->u.ull; break;
                            case LogType::FLOAT:
                                file << next->u.f; break;
                            case LogType::DOUBLE:
                                file << next->u.d; break;
                        }

                        // update the queue and let the thread go on cooldown
                        queue.updateReadIndex();
                        using namespace std::chrono_literals;
                        std::this_thread::sleep_for(1ms); 
                    }
                }
            }

            explicit Logger(const std::string &file_name) : filename(file_name), queue(LOG_QUEUE_SIZE) {
                // The filename and queue have been initialized
                // now we have to create a new file corresponding to this logger and start the logging thread

                // open the file
                file.open(filename);
                ASSERT(file.is_open(), "Could not open log file " + filename); // remember that macros.h was included in "logtype.h"

                // start the logger thread
                // note that we need: a core to pin on (optiona), a thread name, func to execute, and its corresponding args (optional)
                logger_thread = createAndStartThread(-1, "logger_thread", [this]() { flushQueue(); } ); // flushQueue will write all logs to the open file
                ASSERT(logger_thread != nullptr, "Logger thread did not start");
            }

            ~Logger() {
                std::cerr << "Closing logger... " << std::endl;

                // First, we have to make sure that everything that should be logged is written (some apps skip this for performance)
                while (queue.size()) {
                    using namespace std::chrono_literals;
                    std::this_thread::sleep_for(1s); // note the longer duration than the logger cooldown of 1ms
                }

                // Next, we should stop the logger's runtime to allow flushQueue to exit
                running = false;

                // Now, let the thread exit
                logger_thread->join();

                // Lastly, we close the logging file
                file.close();
            }


            // Also want to stop the Logger class from being misused, so we delete constructors
            Logger() = delete; // non-parameterized constructor
            Logger(const Logger &) = delete; // copy contructor
            Logger(const Logger &&) = delete; // move constructor
            Logger& operator=(const Logger &) = delete; // copy assignment constructor
            Logger& operator=(const Logger &&) = delete; // move assignment constructor
    };



}
