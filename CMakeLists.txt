cmake_minimum_required(VERSION 3.24)
# Name project & enable C++ compiler detection.
project(mastertrader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define a flag that can be passed to cmake.
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

# Fetch the googletest source.
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# Include the gtest build targets in my project.
FetchContent_MakeAvailable(googletest)

# Enable CTest support.
enable_testing()

# Define exchange_tests built from tests/setup_test.cpp.
add_executable(exchange_tests tests/setup_test.cpp)
# Connect the test executable to GTest.
target_link_libraries(exchange_tests PRIVATE GTest::gtest_main)
target_compile_options(exchange_tests PRIVATE -Wall -Wextra -Wpedantic)

if(ENABLE_SANITIZERS)
  # Link the sanitizer runtime support when enabled.
  target_compile_options(exchange_tests PRIVATE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=undefined
  )
  target_link_options(exchange_tests PRIVATE
    -fsanitize=address,undefined
  )
endif()

include(GoogleTest)
gtest_discover_tests(exchange_tests)
