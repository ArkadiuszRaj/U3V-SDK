cmake_minimum_required(VERSION 3.16)
project(u3v_xml_parser CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- pugixml dependency ---
include(FetchContent)
FetchContent_Declare(
    pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
)
FetchContent_MakeAvailable(pugixml)

# --- XML parser library (header-only, but we define a target for dependencies) ---
add_library(xml_parser INTERFACE)
target_include_directories(xml_parser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(xml_parser INTERFACE pugixml)

# --- load_xml: Parse GenICam XML and print node map ---
add_executable(load_xml load_xml.cpp)
target_link_libraries(load_xml PRIVATE xml_parser)

# --- dump_device_xml: Download XML from camera (Linux only, needs u3v driver) ---
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(dump_device_xml dump_device_xml.cpp)

    add_executable(scanner scanner.cpp)
endif()
