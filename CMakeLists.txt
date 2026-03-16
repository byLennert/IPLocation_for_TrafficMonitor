cmake_minimum_required(VERSION 3.10)
project(IPLocationPlugin)

set(CMAKE_CXX_STANDARD 17)

add_library(IPLocationPlugin SHARED
    IPLocationPlugin.cpp
    IPLocationPlugin.h
    IPLocationItem.cpp
    IPLocationItem.h
    NetworkHelper.cpp
    NetworkHelper.h
    HtmlParser.cpp
    HtmlParser.h
    PluginInterface.h
)

target_link_libraries(IPLocationPlugin PRIVATE winhttp iphlpapi)

# Define UNICODE for Windows
target_compile_definitions(IPLocationPlugin PRIVATE UNICODE _UNICODE)

if (MSVC)
    target_compile_options(IPLocationPlugin PRIVATE /utf-8)
endif()
