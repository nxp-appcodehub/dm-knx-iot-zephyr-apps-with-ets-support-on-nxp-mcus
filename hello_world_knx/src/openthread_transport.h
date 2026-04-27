/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef OPENTHREAD_TRANSPORT_H
#define OPENTHREAD_TRANSPORT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if the device is connected to the network (Openthread)
 *
 * This function verifies that the OpenThread instance is available and
 * that the device has at least achieved the CHILD role in the Thread network.
 *
 * @return true if connected to the network
 * @return false if not connected or other (instance not available)
 */
bool knx_is_network_connected(void);

/**
 * @brief Wait for network connectivity
 *
 * This function enables waiting for network to be connected
 * This is a blocking call that should be used during initialization.
 *
 * @note This function sleeps for 100ms between checks
 */
void knx_wait_for_network_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENTHREAD_TRANSPORT_H */
