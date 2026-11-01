cmake_minimum_required(VERSION 3.15)
project(meaningful C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS_RELEASE "-O3 -Wall -Wextra")
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_EXE_LINKER_FLAGS "-static")

set(BIN_DIR ${CMAKE_BINARY_DIR}/bin)
set(BUILD_DIR ${CMAKE_BINARY_DIR}/build)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILD_DIR})

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRCS)

add_executable(meaningful ${SRCS}
        include/err_handler.h
        src/err_handler.c)

target_include_directories(meaningful PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(meaningful PRIVATE m)
