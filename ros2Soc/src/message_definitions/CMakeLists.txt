cmake_minimum_required(VERSION 3.8)
project(message_definitions)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(builtin_interfaces REQUIRED)  # Ensure builtin_interfaces is found

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/ChatMessage.msg"
  DEPENDENCIES builtin_interfaces  # Declare the dependency here
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_pytest REQUIRED)
  find_package(ament_cmake_xmllint REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
