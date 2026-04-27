/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <openthread/dataset.h>
#include <openthread/thread.h>
#include <openthread.h>
#include <zephyr/kernel.h>

#define TIMEOUT 100

/**
 * @brief Check if the device is connected to the network (Openthread)
 *
 * This function verifies that the OpenThread instance is available and
 * that the device has at least achieved the CHILD role in the Thread network.
 *
 * @return true if connected to the network
 * @return false if not connected or other (instance not available)
 */
bool knx_is_network_connected(void)
{
	otInstance *instance = openthread_get_default_instance();

	if (instance == NULL) {
		return false;
	}

	/* Device must be at least a CHILD to be considered connected */
	return (otThreadGetDeviceRole(instance) >= OT_DEVICE_ROLE_CHILD);
}

/**
 * @brief Wait for network connectivity
 *
 * This function enables waiting for network to be connected
 * This is a blocking call that should be used during initialization.
 *
 * @note This function sleeps for 100ms between checks
 */
void knx_wait_for_network_ready(void)
{
	otInstance *instance = NULL;

	do {
		instance = openthread_get_default_instance();
		k_msleep(TIMEOUT);
	} while (instance == NULL || otThreadGetDeviceRole(instance) < OT_DEVICE_ROLE_CHILD);
}

