cmake_minimum_required(VERSION 3.21)
project(IPLocationPlugin LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "IPLocationPlugin uses WinHTTP and can only be built on Windows")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_library(IPLocationPlugin SHARED
    IPLocationPlugin.cpp
    IPLocationPlugin.h
    IPLocationItem.cpp
    IPLocationItem.h
    NetworkHelper.cpp
    NetworkHelper.h
    PluginInterface.h
    IPLocationPlugin.def
)

target_link_libraries(IPLocationPlugin PRIVATE winhttp)

# Define UNICODE for Windows
target_compile_definitions(IPLocationPlugin PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

if (MSVC)
    target_compile_options(IPLocationPlugin PRIVATE /utf-8 /W4)
endif()
