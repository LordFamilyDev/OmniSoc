cmake_minimum_required(VERSION 3.10)
project(CPP_OmniSoc)

set(CMAKE_CXX_STANDARD 14)

find_package(Boost REQUIRED)

add_library(OmniSoc STATIC UART_Serial.cpp BLE_Serial.cpp Socket_Serial.cpp)

target_include_directories(OmniSoc PUBLIC ${Boost_INCLUDE_DIRS})

add_executable(ChatClient ChatClient.cpp)
target_link_libraries(ChatClient PRIVATE OmniSoc)

add_executable(UART_Serial_Tester UART_Serial_Tester.cpp)
target_link_libraries(UART_Serial_Tester PRIVATE OmniSoc)