cmake_minimum_required(VERSION 3.10)
project(deque)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/code.cpp")
    add_executable(code code.cpp)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(code main.cpp)
else()
    # Fallback to compiling tests/one/code.cpp just in case
    add_executable(code tests/one/code.cpp)
endif()
