.. zephyr:code-sample:: hello_world_knx
   :name: KNX IoT Hello World

   Basic KNX IoT "Hello World" sample demonstrating KNX IoT initialization,
   networking setup and behavior for KNX IoT light switched actuator basic
   (LSAB) and light switched sensor basic (LSSB) application types.

Overview
********

This sample demonstrates a minimal KNX IoT application. It initializes the
KNX IoT stack, configures basic networking, and exercises the KNX IoT light
switched actuator basic (LSAB) and light switched sensor basic (LSSB) application
types, based on selected application configuration.

Supported application configurations
************************************

- KNX IoT light switched actuator basic (LSAB) using prj_actuator.conf

The actuator example demonstrates:
- Receiving KNX IoT messages
- Controlling outputs (e.g., LEDs)
- Responding to configuration changes from ETS
- Multi-channel support (platform dependent)
- Control KNX IoT stack through command line interface (CLI)
- Control KNX IoT stack programming mode state through long button press

**Features:**
- Group Object Table (GOT) support
- Group Recipient Table (GRT) support
- OSCORE security
- CoAP-based communication

- KNX IoT light switched sensor basic (LSSB) using prj_sensor.conf

The sensor example demonstrates:
- Reading sensor inputs (e.g., button presses)
- Publishing KNX IoT messages
- Responding to configuration changes from ETS
- Multi-channel support (platform dependent)
- Control KNX IoT stack through command line interface (CLI)
- Control KNX IoT stack programming mode state through long button press

**Features:**
- Group Object Table (GOT) support
- Group Publisher Table (GPT) support
- OSCORE security
- CoAP-based communication

It can be used with any :ref:`supported KNX IoT-capable board <boards>` in Zephyr,
including:

- :ref:`frdm_rw612`
- :ref:`frdm_mcxw72`

Building for FRDM-RW612
***********************

To build the KNX IoT Light Switched Actuator Basic (LSAB) application:

.. zephyr-app-commands::
   :zephyr-app: hello_world_knx
   :board: frdm_rw612
   :gen-args: -DEXTRA_CONF_FILE="prj_actuator.conf;prj-ot-knx-iot.conf"
   :goals: build
   :compact:

To build the KNX IoT Light Switched Sensor Basic (LSSB) application:

.. zephyr-app-commands::
   :zephyr-app: hello_world_knx
   :board: frdm_rw612
   :gen-args: -DEXTRA_CONF_FILE="prj_sensor.conf;prj-ot-knx-iot.conf"
   :goals: build
   :compact:

Building for FRDM-MCXW72
************************

To build the KNX IoT Light Switched Actuator Basic (LSAB) application:

.. zephyr-app-commands::
   :zephyr-app: hello_world_knx
   :board: frdm_mcxw72/mcxw727c
   :gen-args: -DEXTRA_CONF_FILE="prj_actuator.conf;prj-ot-knx-iot.conf"
   :goals: build
   :compact:

To build the KNX IoT Light Switched Sensor Basic (LSSB) application:

.. zephyr-app-commands::
   :zephyr-app: hello_world_knx
   :board: frdm_mcxw72/mcxw727c
   :gen-args: -DEXTRA_CONF_FILE="prj_sensor.conf;prj-ot-knx-iot.conf"
   :goals: build
   :compact:

Flashing
********

After building, flash the application to your board:

.. code-block:: bash

   west flash

For MCXW72 boards, ensure the NBU (Network Co-Processor) firmware is flashed first.
Refer to the board documentation for specific flashing requirements.

Running
*******

Power up the board via USB. Connect to the board's serial console (baud rate: 115200 bps) to see the device logs and access the CLI commands.
Issue "knx_iot help" to see the available commands:

.. code-block:: console
uart:~$ knx_iot help

knx_iot - KNX IoT commands

Subcommands:

  ia            : Manage device individual address

  iid           : Manage device installation id

  fid           : Manage fabric identifier

  pm            : Manage programming mode

  lsm           : Manage load state machine

  factoryreset  : Perform factory reset

  sn            : Display serial number

  qr            : Display QR code

  name          : Display application name


Specific application related behavior at runtime:
- **KNX IoT light switched actuator basic (LSAB)**
  - Specific to this application, at boot, LED will flash two times to indicate initialization is complete (1 second on, 1 second off)
  - Long pressing the programming button will trigger LED to flash three times to indicate programming mode is activated (0.5 seconds on, 0.5 seconds off)
  - After device is configured through ETS6 tool, LED will toggle on or off when receiving KNX IoT messages on configured group addresses

- **KNX IoT light switched sensor basic (LSSB)**
  - Long pressing the programming button will trigger LED to flash three times to indicate programming mode is activated (0.5 seconds on, 0.5 seconds off)
  - After device is configured through ETS6 tool, pressing the sensor button will send KNX IoT messages on configured group addresses