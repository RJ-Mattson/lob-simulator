cmake_minimum_required(VERSION 3.15)
project(lob_simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(order_book src/order_book.cpp)
target_include_directories(order_book PUBLIC include)

add_executable(lob_sim src/main.cpp)
target_link_libraries(lob_sim PRIVATE order_book)

enable_testing()
add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE order_book)
add_test(NAME test_order_book COMMAND test_order_book)
