set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-variable")
set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB SOURCES "*.cpp")

include_directories(${PROJECT_SOURCE_DIR})

add_library(libutils STATIC ${SOURCES})

list(APPEND LIBS libutils)
list(APPEND LIBS pthread)