cmake_minimum_required(VERSION 3.20)
project(order_cache LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(order_cache_test order_cache.h order_cache.cpp order_cache_test.cpp)
target_link_libraries(order_cache_test PRIVATE GTest::gtest Threads::Threads)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT order_cache_test)

enable_testing()
add_test(NAME order_cache_test COMMAND order_cache_test)
