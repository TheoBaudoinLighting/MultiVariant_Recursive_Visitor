cmake_minimum_required(VERSION 3.16)
project(MultiVariantRecursiveVisitation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

add_library(multi_visit INTERFACE)
target_include_directories(multi_visit INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(multi_visit INTERFACE cxx_std_17)

add_executable(MultiVariantRecursiveVisitation main.cpp)
target_link_libraries(MultiVariantRecursiveVisitation PRIVATE multi_visit)

enable_testing()

add_executable(
  multi_dispatch_tests
  tests/multi_dispatch_tests.cpp
)

target_link_libraries(
  multi_dispatch_tests
  multi_visit
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(multi_dispatch_tests)
