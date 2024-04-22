#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <chrono>
#include <ctime>
#include <string>


namespace Common {

    /*
        time_utils.h includes useful functions for us to fetch the current time of the system and convert it into strings we can use.
    */

   typedef int64_t Nanos; // int64_t is from the chrono library

   // Unit conversions
   constexpr Nanos NANOS_TO_MICROS = 1000;
   constexpr Nanos MICROS_TO_MILLIS = 1000;
   constexpr Nanos MILLIS_TO_SECONDS = 1000;
   constexpr Nanos NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
   constexpr Nanos NANOS_TO_SECONDS = NANOS_TO_MILLIS * MILLIS_TO_SECONDS;

   /*
        Current time elapsed since the 'beginning of time' (usually Jan 1, 1970)
        1. std::chrono::system_clock::now() gives us the current time
        2. .time_since_epoch() gives us the amount of time passed since the system clock began
        3. std::chrono::duration_cast<std::chrono::nanoseconds>() puts this time into nanoseconds
        4. .count() gives us the count of the nanoseconds
   */
   inline auto getCurrentNanos() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
           ).count();
   }

   inline auto getCurrentTimeStr(std::string* time_str) {
        const auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // getting the time into a std::time_t data type
        time_str->assign(ctime(&time));

        // ctime() adds a newline at the end, which we want to get rid of and make the string null-terminated instead
        if (!time_str->empty()) {
            time_str->at(time_str->length() - 1) = '\0';

        }

        // Notice that we return a reference to the string object and not a pointer to it, this is a common pattern for usability for code calling this func
        return *time_str; 
   }


    
}





#endif