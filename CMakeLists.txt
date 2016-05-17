cmake_minimum_required(VERSION 2.8)

project(CONCURRENT)

SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--build-id")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99") # Add c99 functionality
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11") # Add c++11 functionality


get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

if (${LIB64} STREQUAL "TRUE")
    set(LIBSUFFIX 64)
else()
    set(LIBSUFFIX "")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake-modules)

find_package(Exception REQUIRED)
find_package(CxxTest REQUIRED)

ENABLE_TESTING()

add_subdirectory(src)
