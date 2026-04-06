cmake_minimum_required(VERSION 3.20)
project(u3v-sdk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# pugixml (submodule)
add_subdirectory(import/pugixml EXCLUDE_FROM_ALL)

# Header-only SDK library (depends on pugixml via xml_parser/NodeMap.hpp)
add_library(u3v_sdk INTERFACE)
target_include_directories(u3v_sdk INTERFACE include)
target_link_libraries(u3v_sdk INTERFACE pugixml)

# XML parser tools
add_executable(load_xml include/xml_parser/tools/load_xml.cpp)
target_include_directories(load_xml PRIVATE include/xml_parser)
target_link_libraries(load_xml PRIVATE pugixml)

add_executable(dump_device_xml include/xml_parser/tools/dump_device_xml.cpp)
add_executable(scanner include/xml_parser/tools/scanner.cpp)

# Example application
add_executable(u3v_example examples/grab_frames.cpp)
target_link_libraries(u3v_example PRIVATE u3v_sdk)
