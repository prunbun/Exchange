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
                // note that we need: a core to pin on (optional), a thread name, func to execute, and its corresponding args (optional)
                logger_thread = createAndStartThread(-1, "logger_thread", [this]() { flushQueue(); } ); // flushQueue will write all logs to the open file
                ASSERT(logger_thread != nullptr, "Logger thread did not start");
            }

            ~Logger() {
                std::cerr << "Closing logger... " << std::endl;

                // First, we have to make sure that everything that should be logged is written (some apps skip this for performance)
                while (queue.size()) {
                    using namespace std::chrono_literals;
                    std::cout << queue.size() << std::endl;
                    std::this_thread::sleep_for(1s);
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

            
            // main way to push to logger queue
            void pushToLoggerQueue(const LogElement &log_element) noexcept {
                *(queue.getNextWriteTo()) = log_element;
                queue.updateWriteIndex();
            } 

            // Overload pushValue for different data types

            // 1.1 char
            void pushValue(const char value) noexcept {
                pushToLoggerQueue(LogElement{LogType::CHAR, {.c = value}});
            }

            // 1.2 char array
            void pushValue(const char *value) {
                while (*value) {
                    pushValue(*value);
                    ++value;
                }
            }

            // 1.3 string
            void pushValue(const std::string &value) noexcept {
                pushValue(value.c_str());
            }

            // int
            void pushValue(const int value) noexcept {
                pushToLoggerQueue(LogElement{LogType::INTEGER, {.i = value}});
            }

            // long int
            void pushValue(const long int value) noexcept {
                pushToLoggerQueue(LogElement{LogType::LONG_INTEGER, {.l = value}});
            }

            // long long int
            void pushValue(const long long int value) noexcept {
                pushToLoggerQueue(LogElement{LogType::LONG_LONG_INTEGER, {.ll = value}});
            }

            // unsigned int
            void pushValue(const unsigned int value) noexcept {
                pushToLoggerQueue(LogElement{LogType::UNSIGNED_INTEGER, {.u = value}});
            }

            // unsigned long int
            void pushValue(const unsigned long int value) noexcept {
                pushToLoggerQueue(LogElement{LogType::UNSIGNED_LONG_INTEGER, {.ul = value}});
            }

            // unsigned long long int
            void pushValue(const unsigned long long int value) noexcept {
                pushToLoggerQueue(LogElement{LogType::UNSIGNED_LONG_LONG_INTEGER, {.ull = value}});
            }

            // float
            void pushValue(const float value) noexcept {
                pushToLoggerQueue(LogElement{LogType::FLOAT, {.f = value}});
            }

            // double
            void pushValue(const double value) noexcept {
                pushToLoggerQueue(LogElement{LogType::DOUBLE, {.d = value}});
            }

            /*
                API for main thread to use logger
            */

           // logging a format string
           template<typename T, typename... Vars>
           void log(const char *format_string, const T &value, Vars... remaining_vars) noexcept {
                // while we have more to log
                while (*format_string) {

                    // is this an escape/variable?
                    if (*format_string == '%') {

                        // if we want to actually log a '%' character, then, we would escape it by doing '%%', 
                        // so we skip this one, and log the next '%' after this 
                        if(UNLIKELY(*(format_string + 1) == '%')) {
                            ++format_string;
                        } else {
                            pushValue(value);
                            log(format_string + 1, remaining_vars...); // <- recursively call log to 'consume' this value
                            return;
                        }
                    }

                    pushValue(*format_string++); // <- notice how this is unreachable if there was a variable
                }

                FATAL("printf log() did not include enough % keywords to print out all variables");
           }

           // logging a plain string
           // notice that this function will also be called when all variables are logged in the previous log() function
           void log(const char *message) noexcept {
                while(*message) {
                    if (*message == '%') {
                        if (UNLIKELY(*(message + 1) == '%')) {
                            ++message; // <- follows same logic as above for escaping '%'
                        } else {
                            // std::cout << *(message - 16) << *(message - 15) << *(message - 14) << *(message - 13) << *(message - 12) << *(message - 11) << *(message - 10) << *(message - 9) << *(message - 8) << *(message - 7) << *(message - 6) << *(message - 5) << *(message - 4) << *(message - 3) << *(message - 2) << *(message - 1) << std::endl; 
                            FATAL("Found a printf keyword in the plain string log(), but variables were not provided");
                        }
                    }

                    pushValue(*message++);
                }
           }


    };



}
