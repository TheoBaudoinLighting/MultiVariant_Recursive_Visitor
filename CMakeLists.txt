cmake_minimum_required(VERSION 3.31)
project(MultiVariantRecursiveVisitation)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(MultiVariantRecursiveVisitation main.cpp)

enable_testing()

add_executable(
  multi_dispatch_tests
  tests/multi_dispatch_tests.cpp
)

target_link_libraries(
  multi_dispatch_tests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(multi_dispatch_tests)
