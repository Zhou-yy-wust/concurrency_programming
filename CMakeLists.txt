cmake_minimum_required(VERSION 3.22.1)
project(ConcurrencyProgramming)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Threads REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_singleton test_singleton.cpp Singleton.h)
target_link_libraries(test_singleton Threads::Threads)


add_executable(test_stack test_stack.cpp Stack.h SpscQueue.h)
target_link_libraries(test_stack atomic)
target_link_libraries(test_stack Threads::Threads)

add_executable( test_sq  test_sq.cpp SpscQueueUtils.h SpscQueueUtils.h)
target_link_libraries(test_sq atomic)
target_link_libraries(test_sq Threads::Threads)
