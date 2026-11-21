cmake_minimum_required(VERSION 3.10.3)

project(echo_server VERSION 1.0.0 LANGUAGES C)


# Defaults ####
option(ENABLE_IPV6 "Enable IPv6" ON)
option(ENABLE_UDP "Enable UDP" ON)

if(NOT DEFINED LISTEN_PORT)
    set(LISTEN_PORT 7)
endif()

if(NOT DEFINED LISTEN_BACKLOG)
    set(LISTEN_BACKLOG 50)
endif()

if(NOT DEFINED MAX_CLIENTS)
    set(MAX_CLIENTS 10)
endif()

if(NOT DEFINED BUFFER_SIZE)
    set(BUFFER_SIZE 128)
endif()

if (ENABLE_IPV6)
    add_compile_definitions(-DENABLE_IPV6)
endif()

if (ENABLE_UDP)
    add_compile_definitions(-DENABLE_UDP)
endif()

add_definitions(
    -DLISTEN_PORT=${LISTEN_PORT}
    -DLISTEN_BACKLOG=${LISTEN_BACKLOG}
    -DMAX_CLIENTS=${MAX_CLIENTS}
    -DBUFFER_SIZE=${BUFFER_SIZE}
)


# Build types ####
string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
if(build_type STREQUAL "release")
    message("[*] Building Release")
    add_compile_options(-O3 -g0)

elseif(build_type STREQUAL "debug")
    message("[*] Building Debug")
    add_definitions(-DDEBUG=1)
    add_compile_options(-O0 -g3)

endif()


add_compile_options(-std=c89 -ansi -pedantic -Wall -Wextra)
add_executable(echo_server
    src/server.c
    src/main.c
)

# Tests
include(CTest)
enable_testing()

add_subdirectory(tests)
