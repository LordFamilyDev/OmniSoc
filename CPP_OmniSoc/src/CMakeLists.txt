cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0074 NEW)

project(CPP_OmniSoc)

set(CMAKE_CXX_STANDARD 14)

# Find Boost
find_package(Boost REQUIRED)

# Create a static library
add_library(OmniSoc STATIC UART_Serial.cpp BLE_Serial.cpp Socket_Serial.cpp)

# Include Boost directories
target_include_directories(OmniSoc PUBLIC ${Boost_INCLUDE_DIRS})

# Add executable for ChatClient and link against OmniSoc
add_executable(ChatClient ChatClient.cpp)
target_link_libraries(ChatClient PRIVATE OmniSoc)

# Add executable for UART_Serial_Tester and link against OmniSoc
add_executable(UART_Serial_Tester UART_Serial_Tester.cpp)
target_link_libraries(UART_Serial_Tester PRIVATE OmniSoc)

# Find and link Threads library (cross-platform)
find_package(Threads REQUIRED)

# Conditionally link pthread on Linux
if (UNIX AND NOT APPLE)
    target_link_libraries(UART_Serial_Tester PRIVATE ${CMAKE_THREAD_LIBS_INIT} pthread)
    target_link_libraries(OmniSoc PRIVATE ${CMAKE_THREAD_LIBS_INIT} pthread)
else()
#This is somewhat unneccesary as threading is inherently linked on windows systems without explicit callout.
    target_link_libraries(UART_Serial_Tester PRIVATE ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(OmniSoc PRIVATE ${CMAKE_THREAD_LIBS_INIT})
endif()