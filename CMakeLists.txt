cmake_minimum_required(VERSION 3.5.1)
project(neat_test_suite)

# Version numbers
set(neat_test_suite_VERSION_MAJOR 1)
set(neat_test_suite_VERSION_MINOR 0)

# Set specific include and library locations for FreeBSD
IF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    SET(CMAKE_REQUIRED_INCLUDES "/usr/local/include" "/usr/include")
    SET(CMAKE_LIBRARY_PATH "/usr/local/lib")
    INCLUDE_DIRECTORIES("/usr/local/include")
ENDIF()
    
# Include directories
include_directories(include)

# Compile options
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -fno-omit-frame-pointer -g")

#
# Specify the source file components of each application
#

# NEAT applications
set(NEAT_SERVER_SOURCES src/neat_server.c src/common.c src/server_common.c src/http_common.c src/util_new.c src/picohttpparser.c)
set(NEAT_CLIENT_SOURCES src/neat_client.c src/common.c src/client_common.c src/picohttpparser.c src/http_common.c src/util_new.c)

# libuv applications
set(LIBUV_SERVER_SOURCES src/libuv_server.c src/common.c src/socket_common.c src/server_common.c src/http_common.c src/util_new.c src/picohttpparser.c)
set(LIBUV_CLIENT_SOURCES src/libuv_client.c src/common.c src/socket_common.c src/client_common.c src/http_common.c src/util_new.c src/picohttpparser.c)

# kqueue applications
set(KQUEUE_SERVER_SOURCES src/kqueue_server.c src/common.c src/socket_common.c src/server_common.c src/http_common.c src/util_new.c src/picohttpparser.c)
set(KQUEUE_CLIENT_SOURCES src/kqueue_client.c src/common.c src/socket_common.c src/client_common.c src/http_common.c src/util_new.c src/picohttpparser.c)

# Find path to the required shared libraries
find_library(UV_LIB uv)
find_library(NEAT_LIB neat)

#
# Build NEAT applications
#
add_executable(neat_server ${NEAT_SERVER_SOURCES})
target_link_libraries(neat_server ${NEAT_LIB} ${UV_LIB} rt)
add_executable(neat_client ${NEAT_CLIENT_SOURCES})
target_link_libraries(neat_client ${NEAT_LIB} ${UV_LIB} m rt)

#
# Build libuv applications
#
add_executable(libuv_server ${LIBUV_SERVER_SOURCES})
target_link_libraries(libuv_server ${UV_LIB})
add_executable(libuv_client ${LIBUV_CLIENT_SOURCES})
target_link_libraries(libuv_client ${UV_LIB})

#
# Build kqueue applications
#
IF (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  add_executable(kqueue_server ${KQUEUE_SERVER_SOURCES})
  add_executable(kqueue_client ${KQUEUE_CLIENT_SOURCES})
ENDIF()
