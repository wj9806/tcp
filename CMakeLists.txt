cmake_minimum_required(VERSION 3.7)

project(net)
set(CMAKE_C_STANDARD 99)

LINK_DIRECTORIES(
    ${PROJECT_SOURCE_DIR}/npcap/Lib/x64          # win64 used
)

add_definitions(-D_CRT_SECURE_NO_WARNINGS)

include_directories(
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/npcap/Include          # pcap path
        ${PROJECT_SOURCE_DIR}/app
        ${PROJECT_SOURCE_DIR}/plat
        ${PROJECT_SOURCE_DIR}/net/net
)

file(GLOB_RECURSE SOURCE_LIST
        "app/*.c"
        "app/*.h"
        "plat/*.c"
        "plat/*.h"
        "net/*.h"
        "net/*.c"
)

add_executable(${PROJECT_NAME} ${SOURCE_LIST})

add_definitions(-DNET_DRIVER_PCAP)    # use pcap

message(STATUS "current platform: ${CMAKE_HOST_SYSTEM_NAME}")
if(CMAKE_HOST_SYSTEM_NAME MATCHES "Windows")
    # windows
    add_definitions(-DSYS_PLAT_WINDOWS)
    target_link_libraries(${PROJECT_NAME} wpcap packet Ws2_32)
else()
    # linux and mac
    add_definitions(-DSYS_PLAT_LINUX)
    target_link_libraries(${PROJECT_NAME} pthread pcap)
endif()


