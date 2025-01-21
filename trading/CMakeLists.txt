set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic -Wno-unused-private-field -Wno-unused-parameter -Wno-unused-variable")
set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB SOURCES "*/*.cpp")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/trading)

add_library(libtrading STATIC ${SOURCES} strategy/risk_manager.cpp)