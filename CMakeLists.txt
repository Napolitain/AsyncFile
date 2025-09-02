cmake_minimum_required(VERSION 3.31)

# Check if this is the main project or a subproject
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    # Building as main project
    project(AsyncFile)
    set(CMAKE_CXX_STANDARD 23)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    if (UNIX AND NOT APPLE)
        set(CMAKE_CXX_FLAGS "-O3 -flto -Wpedantic -Wall -Wextra -Wsign-conversion -Wconversion -march=native")
    elseif (WIN32)
        set(CMAKE_CXX_FLAGS "")
    endif ()


    # Dependenciesa
    set(CPM_DOWNLOAD_LOCATION ${CMAKE_BINARY_DIR}/CPM.cmake)
    file(DOWNLOAD
            https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/cpm.cmake
            ${CPM_DOWNLOAD_LOCATION})
    include(${CPM_DOWNLOAD_LOCATION})

    set(ASYNCFILE_IS_MAIN_PROJECT TRUE)
else ()
    # Building as subproject
    set(ASYNCFILE_IS_MAIN_PROJECT FALSE)
endif ()

message(STATUS "Building AsyncFile")

file(GLOB_RECURSE SOURCES src/*.cpp)

add_library(libAsyncFile STATIC)
target_sources(libAsyncFile PRIVATE ${SOURCES})
set_target_properties(libAsyncFile PROPERTIES OUTPUT_NAME AsyncFile)
target_include_directories(libAsyncFile PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (UNIX AND NOT APPLE)  # Ensures it's Linux, not macOS
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBURING REQUIRED liburing)

    target_include_directories(libAsyncFile PUBLIC ${LIBURING_INCLUDE_DIRS})
    target_link_libraries(libAsyncFile PRIVATE ${LIBURING_LIBRARIES})
endif ()

option(ASYNCFILE_BUILD_EXAMPLES "Build AsyncFile examples" ON)
if (ASYNCFILE_IS_MAIN_PROJECT OR ASYNCFILE_BUILD_EXAMPLES)
    add_executable(AsyncFile_example examples/main.cpp)
    target_link_libraries(AsyncFile_example libAsyncFile)
endif ()

option(ASYNCFILE_BUILD_TESTS "Build AsyncFile tests" ON)
if (ASYNCFILE_IS_MAIN_PROJECT OR ASYNCFILE_BUILD_TESTS)
    add_subdirectory(tests)
endif ()