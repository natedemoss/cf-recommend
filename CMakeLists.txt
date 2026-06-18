cmake_minimum_required(VERSION 3.16)
project(cf_recommend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- dependencies -----------------------------------------------------------
find_package(CURL REQUIRED)

# SQLite3 ships a CMake module on most platforms; fall back to a manual search.
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    find_path(SQLite3_INCLUDE_DIRS sqlite3.h)
    find_library(SQLite3_LIBRARIES NAMES sqlite3)
    if(NOT SQLite3_INCLUDE_DIRS OR NOT SQLite3_LIBRARIES)
        message(FATAL_ERROR "SQLite3 not found")
    endif()
endif()

# --- target -----------------------------------------------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_executable(cf ${SOURCES})

target_include_directories(cf PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(cf PRIVATE
    CURL::libcurl
    ${SQLite3_LIBRARIES}
)

if(MSVC)
    target_compile_options(cf PRIVATE /W4 /utf-8)
else()
    target_compile_options(cf PRIVATE -Wall -Wextra)
endif()
