cmake_minimum_required(VERSION 3.25)
project(GeoBRGdps VERSION 1.0.3 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Same NDK r28 compatibility shim as the main GeoBR mod.
# NDK r28 libc++ shim: Android only (clang-cl would read the path as a source file).
if(ANDROID)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/../src/compat/NdkStdCompat.hpp>"
    )
endif()
add_library(${PROJECT_NAME} SHARED src/main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ../src)
if(NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Point it to the Geode SDK directory.")
endif()
add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME} DONT_INSTALL)
