.. _board_support:

Supporting New Boards
#####################

The Hubble Device SDK separates satellite support into three layers:

* the common satellite module in ``src``;
* the RTOS or framework port in ``port/<rtos>``;
* the board and SoC implementation in ``port/<rtos>/boards``.

Most new board work belongs in the board and SoC layer. Do not add board
conditionals to ``src``. Common satellite code calls the radio port contract in
``include/hubble/port/sat_radio.h``. The Zephyr, FreeRTOS, and ESP-IDF satellite
ports already implement that contract and delegate the hardware-specific work
to board functions declared in ``port/<rtos>/sat_board.h``.

Choosing an Integration Path
****************************

Use one of the existing ports when the new board runs on an already-supported
environment:

* Zephyr: add support under ``port/zephyr/boards``.
* ESP-IDF: add support under ``port/esp-idf/hubblenetwork-sdk/boards``.
* FreeRTOS: add support under ``port/freertos/boards`` and include it from
  ``port/freertos/hubblenetwork-sdk.mk``.

Only create or change a top-level port when the target cannot use the existing
Zephyr, ESP-IDF, or FreeRTOS integration. A new top-level port must implement
the contracts under ``include/hubble/port``.

Port Contract APIs
==================

If you are using an unsupported RTOS or framework port, implement the
public satellite radio contract in ``include/hubble/port/sat_radio.h``:

* ``hubble_sat_port_init()``
* ``hubble_sat_port_packet_send()``
* ``hubble_sat_dtm_port_packet_send()``
* ``hubble_sat_dtm_port_power_set()``
* ``hubble_sat_dtm_port_cw_start()``
* ``hubble_sat_dtm_port_cw_stop()``

The existing Zephyr and FreeRTOS implementations are useful references:
``port/zephyr/hubble_sat_zephyr.c`` and
``port/freertos/hubble_sat_freertos.c``.

APIs for Existing Ports
***********************

When using an existing satellite port, implement the board API consumed by that
port. The declarations are in ``port/zephyr/sat_board.h`` and
``port/freertos/sat_board.h``. The ESP-IDF component reuses the FreeRTOS
satellite port and includes the same board API.

Required Board APIs
===================

Every satellite board implementation must provide:

``hubble_sat_board_init()``
   Initialize board resources used by satellite transmissions. This is where
   the board should configure FEM GPIOs, timers, radio-driver state, semaphores,
   and any static power defaults. It is called from ``hubble_sat_port_init()``.

``hubble_sat_board_enable()``
   Prepare the board for a satellite transmission. Typical work includes
   enabling clocks, enabling or configuring the radio, restoring SoC radio
   state, setting TX power, and enabling a PA or FEM path.

``hubble_sat_board_disable()``
   Return the board to its idle state after transmission. The function should
   disable temporary radio state and put external RF components into their
   low-power state.

``hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)``
   Transmit the already-framed packet. The common code and RTOS port handle
   packet formatting, retries, and retry spacing before this call. The board
   implementation is responsible for sending each symbol on the frame channel
   using the timing constants from ``include/hubble/port/sat_radio.h``.

These functions return ``0`` on success and a negative ``errno`` value on
failure.

Using a Supported SoC
*********************

If the board uses a SoC family that the SDK already supports, prefer reusing the
SoC implementation and adding only the board-specific wrapper. The SoC layer
owns radio-register programming and symbol transmission. The board wrapper owns
board resources such as FEM setup, PA control, power limits, and board-specific
selection in the build system.

Nordic SoCs on Zephyr
=====================

The Nordic Zephyr implementation is split into a SoC API and board wrappers:

* SoC API: ``port/zephyr/boards/nordic/sat_soc.h``
* nRF52 SoC implementation: ``port/zephyr/boards/nordic/nrf52/nrf52_soc.c``
* nRF53 SoC implementation: ``port/zephyr/boards/nordic/nrf53/nrf53_soc.c``
* nRF54 SoC implementation: ``port/zephyr/boards/nordic/nrf54/nrf54_soc.c``
  or ``nrf54_soc_softdevice.c``

To add a new board that uses one of these supported Nordic SoCs:

#. Add a board wrapper beside the existing board files for that SoC family,
   for example under ``port/zephyr/boards/nordic/nrf54``.
#. Implement the ``hubble_sat_board_*`` functions by calling the matching
   ``hubble_sat_soc_*`` functions from ``sat_soc.h``.
#. Add any board-only RF handling around the SoC calls. For example, configure
   and enable a FEM before ``hubble_sat_soc_packet_send()`` and put it back to
   sleep afterwards.
#. Update the SoC-family ``CMakeLists.txt`` to compile the board wrapper when
   the board's Zephyr ``CONFIG_BOARD_*`` symbol is selected.

The existing ``nrf54l15dk.c``, ``nrf21540dk.c``, and ``thingy53.c`` files show
the intended pattern.

Silicon Labs EFR32 on Zephyr
============================

Silicon Labs support currently lives under
``port/zephyr/boards/silabs/efr32``. The ``radio.c`` file implements the board
API directly using RAIL, and ``CMakeLists.txt`` selects generated radio
configuration for supported SoC variants. ``CONFIG_SOC_SILABS_XG24`` currently
selects the XG24 radio configuration under ``xg24``.

To add a new EFR32 board that uses the supported XG24 SoC path, add any required
board configuration to the Zephyr application and reuse the existing EFR32
sources. To add a new EFR32 SoC variant, add a generated radio configuration
directory and extend ``port/zephyr/boards/silabs/efr32/CMakeLists.txt`` with the
new SoC selection.

ESP32-C6 on ESP-IDF
===================

The ESP-IDF satellite component currently selects board support when
``IDF_TARGET`` is ``esp32c6``. The implementation lives in
``port/esp-idf/hubblenetwork-sdk/boards/esp32c6/radio.c`` and implements the
same ``hubble_sat_board_*`` API used by the FreeRTOS satellite port.

To support another ESP-IDF target, add a new directory under
``port/esp-idf/hubblenetwork-sdk/boards`` and update the component
``CMakeLists.txt`` to select the include directory and source files for the new
``IDF_TARGET``.

TI CC23xx/CC27xx on FreeRTOS
============================

The FreeRTOS TI board support lives under
``port/freertos/boards/ti/cc23xx_cc27xx``. It is included by
``port/freertos/hubblenetwork-sdk.mk`` when the application ``CFLAGS`` identify
a CC23xx target. The TI makefile selects the radio configuration based on the
``CC23``/``CC27`` and ``USE_DMM`` build flags.

To add a board in this supported TI SoC family, reuse the existing TI board
directory when the radio path and generated SysConfig files match the target. If
the board needs different generated radio settings, add the generated files
under ``radio_config`` and extend
``boards/ti/cc23xx_cc27xx/hubblenetwork-sdk-sat-ti.mk`` to select them.

DTM APIs
********

DTM means Direct Test Mode. It exposes the satellite radio physical layer for
validation, certification, and production testing by sending controlled test
packets or a continuous-wave (CW) carrier on a selected Hubble satellite
channel. DTM does not exercise the normal encrypted application payload flow;
it is a radio test interface used to verify channel selection, transmit power,
packet transmission, and carrier output on hardware.

If ``CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE`` is enabled, the board must also
provide:

``hubble_sat_board_power_set(int8_t power)``
   Set the transmit power used by DTM packet and CW operations. Return
   ``-EINVAL`` when the requested dBm value is not supported by the board.

``hubble_sat_board_cw_start(uint8_t channel)``
   Start a continuous-wave transmission on the requested Hubble satellite
   channel. The port validates the channel range before calling the board API.

``hubble_sat_board_cw_stop()``
   Stop the continuous-wave transmission and leave the board in a state that can
   be disabled by the port.


Board Support Checklist
***********************

For each new board:

#. Confirm whether the board can use an existing RTOS port.
#. Confirm whether the SoC already has an SDK implementation.
#. Implement the required ``hubble_sat_board_*`` functions, reusing
   ``hubble_sat_soc_*`` where available.
#. Add the board source and include paths to the relevant build file.
#. Enable ``CONFIG_HUBBLE_SAT_NETWORK`` in an application or sample build.
#. Enable ``CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE`` and implement the DTM APIs if
   the board needs radio validation, CW testing, or production test support.
#. Build at least one satellite sample for the new board.
#. Exercise packet send and, when enabled, DTM packet, power, and CW operations
   on hardware.
