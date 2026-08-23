cmake_minimum_required(VERSION 3.15)
project(lob_simulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_library(order_book src/order_book.cpp)
target_include_directories(order_book PUBLIC include)

add_library(historical src/lobster_reader.cpp src/replayer.cpp)
target_link_libraries(historical PUBLIC order_book)

add_library(simulator src/simulator.cpp)
target_link_libraries(simulator PUBLIC order_book)

add_executable(lob_sim src/main.cpp)
target_link_libraries(lob_sim PRIVATE order_book)

add_executable(lob_replay src/replay_cli.cpp)
target_link_libraries(lob_replay PRIVATE historical)

add_executable(lob_validate src/validate_replay.cpp)
target_link_libraries(lob_validate PRIVATE historical)

enable_testing()
add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE order_book)
add_test(NAME test_order_book COMMAND test_order_book)

add_executable(test_replayer tests/test_replayer.cpp)
target_link_libraries(test_replayer PRIVATE historical)
add_test(NAME test_replayer COMMAND test_replayer)

add_executable(test_simulator tests/test_simulator.cpp)
target_link_libraries(test_simulator PRIVATE simulator)
add_test(NAME test_simulator COMMAND test_simulator)
