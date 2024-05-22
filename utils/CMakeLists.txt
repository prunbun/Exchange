set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "-std=c++20 -Wall -Wextra -Werror -Wpedantic -Wno-unused-variable")
set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB SOURCES "*.cpp")

include_directories(${PROJECT_SOURCE_DIR})

add_library(libutils STATIC ${SOURCES})