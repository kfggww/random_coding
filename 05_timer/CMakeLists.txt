cmake_minimum_required(VERSION 3.16.3)
project(timer)

set(LIB_TIMER_SRCS ${CMAKE_SOURCE_DIR}/src/timer.cpp)
add_library(timer SHARED ${LIB_TIMER_SRCS})
target_include_directories(timer PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(timer PRIVATE -pthread)
target_link_libraries(timer pthread)

add_executable(timer_test ${CMAKE_SOURCE_DIR}/test/main.cpp)
target_include_directories(timer_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(timer_test timer)