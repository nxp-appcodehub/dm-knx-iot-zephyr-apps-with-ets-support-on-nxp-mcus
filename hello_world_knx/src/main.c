/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2024-2025 KNX Association
 * Copyright 2026 NXP
 */

/**
 * @file main.c
 *
 * @brief KNX IoT Virtual Switching Actuator/Sensor Application
 *
 * This application implements a KNX IoT device that can function as either
 * an actuator (controlling LEDs) or a sensor (sending button state), running
 * on the Zephyr RTOS.
 *
 * ## Application Design
 *
 * ### Architecture
 * - Main thread: Handles initialization and can perform background tasks
 * - KNX thread: Runs the KNX IoT stack event loop
 * - Work queue: Processes button press events asynchronously
 *
 * ### Key Components
 * - **Stack Initialization**: app_initialize_stack()
 * - **Resource Registration**: Registers all KNX endpoints and handlers
 * - **Threading Model**: Separate thread for KNX stack to avoid blocking
 * - **Event Handling**: Button interrupts trigger work items for processing
 *
 * ### Supported Functions
 * - app_init: Initializes the stack values
 * - register_resources: Registers all endpoints with GET/PUT/POST/DELETE handlers
 * - main: Starts the stack with registered resources
 *
 * ### Request Handlers
 * - get_[path]: Called when GET is invoked on [path], returns global variables
 * - put_[path]: Called when PUT is invoked on [path], updates global variables
 *
 */

/* KNX IoT Stack includes */
#include "oc_api.h"
#include "oc_assert.h"
#include "oc_core_res.h"
#include "oc_helpers.h"
#include "oc_knx.h"
#include "oc_knx_client.h"
#include "oc_knx_dev.h"
#include "oc_knx_fp.h"
#include "oc_rep.h"
#ifdef OC_SPAKE
#include "oc_spake2plus.h" // Security enrollment by password
#endif
#include "oc_storage.h"

/* KNX IoT porting layer includes */
#include "oc_clock.h"
#include "dns-sd.h"

/* KNX IoT Application-specific includes */
#include "knx_iot_virtual_knx.h"
#include "app.h"

/* OpenThread support */
#if defined(CONFIG_NET_L2_OPENTHREAD)
#include "openthread_transport.h"
#endif

/* Zephyr RTOS includes */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>

/* Standard C includes */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

void knx_toggle_programming_mode(void);

/*
 * Thread Stack Allocation
 */

/* Thread stack allocation */
K_THREAD_STACK_DEFINE(knx_thread_stack, KNX_THREAD_STACK_SIZE);

/*
 * Application Context Structure
 */

/**
 * @brief Main application context structure
 */
typedef struct {
	/* Hardware resources */
	struct {
		struct gpio_dt_spec led0;          /**< LED0 device specification */
		struct gpio_dt_spec button;        /**< Button device specification */
	} hw;

	/* Threading and synchronization */
	struct {
		struct k_thread thread_data;        /**< KNX thread control block */
		k_tid_t thread_id;                  /**< KNX thread ID */
		atomic_t running;                   /**< Thread running flag */
		struct k_mutex event_mutex;         /**< Event loop mutex */
		struct k_condvar event_condvar;     /**< Event loop condition variable */
	} thread;
} knx_app_context_t;

/*
 * Global Application Context Instance
 */

/**
 * @brief Global application context instance
 *
 * Single global instance containing all application state.
 * All functions should access state through this context.
 */
static knx_app_context_t g_knx_ctx;

/*
 * Application Configuration
 */

/** @brief Application name for KNX device identification */
const char application_name[] = APPLICATION_NAME;

/** @brief Serial number in lowercase for mDNS service publication */
const char sn_lower_case[] = SN_LOWER_CASE;

/*
 * Context Initialization
 */

/**
 * @brief Initialize the application context structure
 *
 * Sets up all context members with their initial values and
 * initializes synchronization primitives.
 *
 * @param ctx Pointer to the application context
 * @return 0 on success, negative error code on failure
 */
static int knx_context_init(knx_app_context_t *ctx)
{
	if (ctx == NULL) {
		return -EINVAL;
	}

	/* Initialize hardware specifications from devicetree */
	ctx->hw.led0 = (struct gpio_dt_spec)GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
	ctx->hw.button = (struct gpio_dt_spec)GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

	/* Initialize threading primitives */
	atomic_set(&ctx->thread.running, 0);
	k_mutex_init(&ctx->thread.event_mutex);
	k_condvar_init(&ctx->thread.event_condvar);

	return 0;
}

/*
 * LED Control Functions
 */

void blink_led(knx_app_context_t *ctx, unsigned int duration_ms, unsigned int repetition_number)
{
	if (ctx == NULL) {
		OC_ERR("Invalid context");
		return;
	}

	const struct gpio_dt_spec *led_dt = &ctx->hw.led0;

	while (repetition_number) {
		/* Toggle LED from one state to the other */
		int ret = gpio_pin_toggle_dt(led_dt);
		if (ret < 0) {
			OC_ERR("Failed to toggle LED0: %d", ret);
			break;
		}

		k_msleep(duration_ms);

		/* Toggle LED back to original state */
		ret = gpio_pin_toggle_dt(led_dt);
		if (ret < 0) {
			OC_ERR("Failed to toggle LED0: %d", ret);
			break;
		}

		k_msleep(duration_ms);

		repetition_number--;
	}
}

/*
 * LED Control Function (Actuator Mode)
 */

#if defined(CONFIG_KNX_ACTUATOR)
/**
 * @brief Update LED state based on KNX channel value
 *
 * This function is called when a PUT request is received on a KNX datapoint.
 * It retrieves the boolean value from the specified channel and updates the
 * corresponding LED.
 *
 * @param channel KNX channel number (0-based)
 * @param point KNX datapoint within the channel
 *
 * @note Currently supports channel 0 (LED0)
 * @warning Logs error if LED control fails
 */
void knx_led_update(uint16_t channel, uint16_t point)
{
	/* Retrieve the boolean value from the KNX channel */
	knx_app_context_t *ctx = &g_knx_ctx;
	bool value = app_retrieve_bool_variable_from_channel(channel, point);
	int ret;

	switch (channel) {
		case 0:
			/* Update LED0 based on KNX value */
			ret = gpio_pin_set_dt(&ctx->hw.led0, value);
			if (ret < 0) {
				OC_ERR("Failed to toggle LED0: %d", ret);
				return;
			}
			break;

		default:
			OC_WRN("Warning: Unsupported LED channel %d (point=%d)\r\n", channel, point);
			break;
	}
}
#endif // CONFIG_KNX_ACTUATOR

/*
 * Button Events
 */

 /**
 * @brief Toggle programming mode of the device via button press
 *
 * This function is called to toggle device programming mode from
 * button press. Tipically, a long button press is required.
 *
 */
void knx_toggle_programming_mode(void)
{
	/* Device not commissioned: Toggle programming mode for ETS discovery */
	oc_device_info_t *device = oc_core_get_device_info();
	bool programming_mode = oc_knx_device_in_programming_mode();

	/* Toggle programming mode state */
	programming_mode = !programming_mode;
	device->pm = programming_mode;

	/* Persist programming mode to storage */
	oc_storage_write(KNX_STORAGE_PM, (uint8_t *)&programming_mode, sizeof(programming_mode));

	/* Update mDNS service advertisement with new programming mode */
	knx_publish_service(oc_string(device->serialnumber), device->iid, device->ia, device->pm);

	PRINT("Programming mode set to: %s\r\n", programming_mode ? "TRUE" : "FALSE");
}

#if defined(CONFIG_KNX_SENSOR)
 /**
 * @brief Send KNX S-mode message for the specific channel and point
 *
 * This function is called to send device button state via KNX S-mode message.
 * This is using as parameters the KNX channel and point to identify the KNX resource.
 *
 * @param channel KNX channel number (0-based)
 * @param point KNX datapoint within the channel
 *
 */
void knx_send_button_state(uint8_t channel, uint8_t point)
{
	if (knx_is_network_connected()) {
		/* Sensor mode: Send button state via KNX S-mode message */

		/* Retrieve URI path for the KNX channel */
		const char *uri_path = app_retrieve_href_from_channel(channel, point);
		bool p = app_retrieve_bool_variable_from_channel(channel, point);
		int ret;

		/* Validate that URI path was successfully retrieved */
		if (uri_path == NULL) {
			OC_ERR("Error: Failed to retrieve URI path for channel %d\r\n", channel);
		} else {
			/* Toggle the current value */
			p = !p;

			/* Update the internal state */
			app_set_bool_variable_from_channel(channel, point, p);

			/* Send KNX S-mode message (multicast or unicast) with normal priority */
			ret = oc_send_s_mode_mc_or_uc_message(OC_SENDER_MULTICAST_SCOPE, uri_path, 'w');
			if (ret < 0) {
				OC_ERR("Failed to send S-mode message: %d", ret);
			}
		}
	} else {
		OC_ERR("No network connection!");
	}
}
#endif // CONFIG_KNX_SENSOR

static void input_event_cb(struct input_event *evt, void *user_data)
{
	/* evt->value has value of 1 when button is pressed and value of 0 when button is released */
	if (evt->sync == 0) {
		// Ignore unsynchronized event
		return;
	}

	/* Events on button press */
	if (evt->value == true) {
		/* Event code is synced with corresponding button */
		switch (evt->code) {
			/* Button short pressed */
			case INPUT_KEY_A:
#if defined(CONFIG_KNX_SENSOR)
				knx_send_button_state(LSSB0, SOO);
#endif /* CONFIG_KNX_SENSOR */
				break;

			/* Button long pressed */
			case INPUT_KEY_F1:
				/* Blink LED to signal programming mode toggle */
				blink_led(&g_knx_ctx, 500, 3);

				/* Toggle programming mode */
				knx_toggle_programming_mode();
				break;

			default:
				return;
		}
	}
}

static const struct device *const longpress_dev = DEVICE_DT_GET(DT_PATH(longpress));
INPUT_CALLBACK_DEFINE(longpress_dev, input_event_cb, NULL);

/*
 * Application-Specific Initialization
 */

/**
 * @brief Initialize application-specific hardware (LED and button)
 *
 * This function configures:
 * - LED0 as output (actuator mode only)
 * - Button as input with interrupt on rising edge
 *
 * @return 0 on success, negative error code on failure
 */
static int knx_hardware_init(knx_app_context_t *ctx)
{
	int ret;

	if (ctx == NULL) {
		return -EINVAL;
	}

	/* ========== LED Initialization ========== */

	/* Verify LED device is ready */
	if (!gpio_is_ready_dt(&ctx->hw.led0)) {
		OC_ERR("Error: LED device %s is not ready\n", ctx->hw.led0.port->name);
		return -ENODEV;
	}

	/* Configure LED pin as output, initially active */
	ret = gpio_pin_configure_dt(&ctx->hw.led0, GPIO_OUTPUT_ACTIVE);
	if (ret != 0) {
		OC_ERR("Error %d: failed to configure LED %s pin %d\n",
			ret, ctx->hw.led0.port->name, ctx->hw.led0.pin);
		return ret;
	}

	/* ========== Button Initialization ========== */

	/* Verify button device is ready */
	if (!gpio_is_ready_dt(&ctx->hw.button)) {
		OC_ERR("Error: button device %s is not ready\n", ctx->hw.button.port->name);
		return -ENODEV;
	}

	/* Configure button pin as input */
	ret = gpio_pin_configure_dt(&ctx->hw.button, GPIO_INPUT);
	if (ret != 0) {
		OC_ERR("Error %d: failed to configure %s pin %d\n",
			ret, ctx->hw.button.port->name, ctx->hw.button.pin);
		return ret;
	}

	PRINT("Set up button at %s pin %d\n", ctx->hw.button.port->name, ctx->hw.button.pin);

	return 0;
}

/*
 * Event Loop Signaling
 */

/**
 * @brief Signal the KNX event loop to wake up
 *
 * This function is called by the KNX stack when an event occurs that requires
 * processing. It wakes up the KNX thread from its sleep state.
 *
 * @note Thread-safe: Uses mutex to protect condition variable signaling
 */
void signal_event_loop(void)
{
	knx_app_context_t *ctx = &g_knx_ctx;

	k_mutex_lock(&ctx->thread.event_mutex, K_FOREVER);
	k_condvar_signal(&ctx->thread.event_condvar);
	k_mutex_unlock(&ctx->thread.event_mutex);
}

/*
 * KNX Thread Implementation
 */

/**
 * @brief KNX thread entry point
 *
 * This thread runs the KNX IoT stack event loop. It:
 * 1. Waits for network readiness (OpenThread if configured)
 * 2. Publishes mDNS service for device discovery
 * 3. Runs the main event loop, processing KNX events
 * 4. Sleeps between events to save power
 *
 * @param p1 Unused parameter
 * @param p2 Unused parameter
 * @param p3 Unused parameter
 */
static void knx_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	knx_app_context_t *ctx = (knx_app_context_t *)p1;
	oc_clock_time_t next_event;

	PRINT("KNX thread started");

	/* Wait for network to be ready before proceeding */
	knx_wait_for_network_ready();

	/* Retrieve device information for service publication */
	const oc_device_info_t *device = oc_core_get_device_info();

	PRINT("serial number: %s", oc_string(device->serialnumber));
	PRINT("host name: %s", oc_string(device->iot_hostname));

	/* Refresh and print IP addresses */
	oc_connectivity_get_endpoints();

	/* Publish mDNS service for device discovery */
	knx_publish_service(oc_string(device->serialnumber), device->iid, device->ia, device->pm);
	PRINT("Server '%s' is now running, waiting on incoming connections...", application_name);

	atomic_set(&ctx->thread.running, 1);

	/* ========== Main Event Loop ========== */
	while (atomic_get(&ctx->thread.running)) {
		/* Poll for next event and get timeout until next scheduled event */
		next_event = oc_main_poll();

		k_mutex_lock(&ctx->thread.event_mutex, K_FOREVER);

		if (next_event == 0) {
			/* No scheduled events: wait indefinitely for signal */
			k_condvar_wait(&ctx->thread.event_condvar, &ctx->thread.event_mutex, K_FOREVER);
		} else {
			/* Calculate time until next event */
			int64_t time_diff = (int64_t)next_event - (int64_t)oc_clock_time();
			k_timeout_t timeout;

			if (time_diff <= 0) {
				timeout = K_NO_WAIT;
			} else if (time_diff > INT32_MAX) {
				timeout = K_MSEC(INT32_MAX);
			} else {
				timeout = K_MSEC((uint32_t)time_diff);
			}
			/* Wait for signal or timeout (whichever comes first) */
			k_condvar_wait(&ctx->thread.event_condvar,
					&ctx->thread.event_mutex,
					timeout);
		}

		k_mutex_unlock(&ctx->thread.event_mutex);
	}

	/* Graceful shutdown of the KNX stack */
	oc_main_shutdown();
	PRINT("KNX thread stopped");
}

/*
 * KNX Thread Management Functions
 */

/**
 * @brief Start the KNX application thread
 *
 * Creates and starts a new thread to run the KNX IoT stack event loop.
 *
 * @param ctx Pointer to the application context
 * @return 0 on success, negative error code on failure
 */
static int knx_thread_start(knx_app_context_t *ctx)
{
	if (ctx == NULL) {
		return -EINVAL;
	}

	ctx->thread.thread_id = k_thread_create(&ctx->thread.thread_data,
						knx_thread_stack,
						K_THREAD_STACK_SIZEOF(knx_thread_stack),
						knx_thread_entry,
						ctx, NULL, NULL,
						KNX_THREAD_PRIORITY, 0, K_NO_WAIT);

	if (ctx->thread.thread_id == NULL) {
		OC_ERR("Failed to create KNX thread");
		return -EAGAIN;
	}

	k_thread_name_set(ctx->thread.thread_id, "knx_app");
	PRINT("KNX thread created successfully");
	return 0;
}

/**
 * @brief Stop the KNX application thread
 *
 * Signals the KNX thread to stop and waits for it to terminate gracefully.
 *
 * @param ctx Pointer to the application context
 */
static void knx_thread_stop(knx_app_context_t *ctx)
{
	if (ctx == NULL) {
		return;
	}

	if (atomic_get(&ctx->thread.running)) {
		/* Signal thread to stop */
		atomic_clear(&ctx->thread.running);
		signal_event_loop();

		/* Wait for thread to terminate */
		k_thread_join(ctx->thread.thread_id, K_FOREVER);
		PRINT("KNX thread joined");
	}
}

/*
 * Main Application Entry Point
 */

/**
 * @brief Main application entry point
 *
 * Initializes the KNX IoT application and starts the KNX thread.
 * The main thread continues to run and can perform other application tasks.
 *
 * Initialization sequence:
 * 1. Initialize hardware (LED, button)
 * 2. Register application callbacks
 * 3. Initialize KNX IoT stack
 * 4. Start KNX thread
 * 5. Enter idle loop (can be used for other tasks)
 *
 * @return 0 on success, negative error code on failure
 *
 * @note In this implementation, main() never returns (infinite loop)
 */
int main(void)
{
	int ret;
	knx_app_context_t *ctx = &g_knx_ctx;

	/* ========== Context Initialization ========== */
	ret = knx_context_init(ctx);
	if (ret < 0) {
		OC_ERR("Context initialization failed with %d, exiting.", ret);
		return ret;
	}

	/* ========== Hardware Initialization ========== */
	ret = knx_hardware_init(ctx);
	if (ret < 0) {
		OC_ERR("Hardware initialization failed with %d, exiting.", ret);
		return ret;
	}

	PRINT("KNX-IOT server name : \"%s\"", application_name);

#if defined(CONFIG_KNX_ACTUATOR)
	/* Register callback for PUT requests on LED actuator channel */
	app_register_put_callback(knx_led_update, NULL);

	/* Blink LED once to signal actuator initialization */
	blink_led(&g_knx_ctx, 1000, 2);
#endif // CONFIG_KNX_ACTUATOR

	/* ========== KNX Stack Initialization ========== */
	const int code = app_initialize_stack(NULL);

	if (code < 0) {
		OC_ERR("stack initialization failed with %d, exiting.", code);
		return code;
	}

	/* ========== Start KNX Thread ========== */
	ret = knx_thread_start(ctx);
	if (ret < 0) {
		OC_ERR("Failed to start KNX thread");
		return ret;
	}

	PRINT("Main thread continues, KNX running in separate thread");

	/* ========== Main Thread Idle Loop ========== */
	/* Main thread can perform other application tasks here */
	while (1) {
		k_sleep(K_SECONDS(10));
		// Main thread can perform other application tasks here
	}

	/* ========== Clean Shutdown (unreachable in this example) ========== */
	knx_thread_stop(ctx);

	return 0;
}
