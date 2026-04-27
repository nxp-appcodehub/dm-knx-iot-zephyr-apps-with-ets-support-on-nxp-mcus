# Copyright 2026 NXP
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Strip qualifiers to get the base board name used in overlay filenames.
string(REGEX REPLACE "/.*$" "" BOARD_BASE "${BOARD}")

# Add prj.conf and specific board conf by default
set(CONF_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf"
)

set(_board_conf "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_BASE}.conf")
if(EXISTS "${_board_conf}")
    list(APPEND CONF_FILE "${_board_conf}")
endif()

if(NOT DEFINED CONFIG_KNXIOT_APP_LSXB_CHANNELS)
    set(CONFIG_KNXIOT_APP_LSXB_CHANNELS 1)
endif()

set(DTC_OVERLAY_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_BASE}_${CONFIG_KNXIOT_APP_LSXB_CHANNELS}_channel.overlay"
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world_knx)

set(SOURCES src/main.c)

# Configure sources for KNX functions that need network input
if(CONFIG_NET_L2_OPENTHREAD)
    list(APPEND SOURCES
        src/openthread_transport.c
    )
endif()

target_sources(app PRIVATE ${SOURCES})

