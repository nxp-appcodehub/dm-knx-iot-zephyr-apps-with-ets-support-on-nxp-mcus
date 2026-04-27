/*
 * Copyright (c) 2022-2023 Cascoda Ltd
 * Copyright (c) 2024-2025 KNX Association
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __cplusplus
extern "C" {
#endif
// network

/*
 * The network router may not allow to send multicast with scope 5 (site local),
 * hence the DEMO applications use scope 2 instead. If needed,
 * sendout with scope 2 and 5 separately may be an option (2 messages).
 */
#define SENDER_SCOPE (2)

// common data
#define NUM_CHANNELS (2)
#define NUM_POINTS (2)
#define SOO (0)
#define IOO (1)
#define LSSB0 (0)
#define LSSB1 (1)
#define LSAB0 (0)
#define LSAB1 (1)
#define EITT (0)

#define FIRMWARE_NAME "KNX stack image"
#define HW_TYPE_ETS6 "000102030405" // 12 string chars, MSB = 00
#define DEV_MODEL_ETS6 "6800" // reuse mask version from iot device
#define MID (0x00FA) // first 4 digits of SN_LOWER_CASE

#if defined(CONFIG_APPLICATION_NAME)
#define APPLICATION_NAME CONFIG_APPLICATION_NAME
#endif

#if defined(CONFIG_SN_LOWER_CASE)
#define SN_LOWER_CASE CONFIG_SN_LOWER_CASE
#endif

#define HOST_NAME_LSAB (SN_LOWER_CASE_LSAB) // default host name (reset uses SN_LOWER_CASE as default)

/* KNX IoT Thread configuration */
/**
 * @brief Stack size for KNX IoT thread (default: 12KB)
 * @note Adjust based on your application's memory requirements
 */
#ifndef KNX_THREAD_STACK_SIZE
#define KNX_THREAD_STACK_SIZE       12*1024
#endif

/**
 * @brief Priority for KNX thread (cooperative scheduling)
 * Lower values = higher priority. Range: 0-15 for cooperative threads
 */
#define KNX_THREAD_PRIORITY         10

/* Work Handler defines */
/**
 * @brief Maximum number of button events that can be queued
 * Prevents event loss during burst button presses
 */
#define BUTTON_ACTION_ARRAY_SIZE    16

/**
 * @brief Size of each button action message (stores pin states)
 */
#define BUTTON_ACTION_LEN           sizeof(uint32_t)

#ifdef __cplusplus
}
#endif




