.. _nrf_audio_eb_smoke:

nRF-Audio-EB smoke test
#######################

.. contents::
   :local:
   :depth: 2

The nRF-Audio-EB smoke test brings up every interface that the :ref:`nrf_audio_eb` shield enables on the current development kit and prints a result line for each.
It verifies wiring and driver bring-up, not audio quality.

Overview
********

The sample walks the shield's interfaces in sequence over the console:

* Blinks the RGB LED (red, green, blue).
* Reads the line-in-detect input.
* Probes the TAC5112 codec and reports whether it acknowledges.
* Mounts a FAT file system on the SD card, then prints its size and root listing.
* Captures one block from the PDM microphone and reports the peak amplitude.
* Configures the audio serial port and clocks out a test tone.

The sample is board-neutral: it addresses the audio serial port through the ``tac5112-i2s`` alias and the microphone through the ``dmic0`` alias, and each interface is compiled out on kits whose shield overlay does not provide it.

Because there is no in-tree TAC5112 driver, the audio step only proves that the serial port initializes and drives BCLK, LRCK and SDOUT.
Audible output requires configuring the codec registers, which the sample does not do.

Requirements
************

The sample supports the following development kits, each fitted with the nRF-Audio-EB and built with ``SHIELD=nrf_audio_eb``.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Development kit
     - Board target
   * - nRF54LM20 DK
     - ``nrf54lm20dk/nrf54lm20a/cpuapp``

See the :ref:`nrf_audio_eb` shield documentation for the per-kit connector and pin assignments.

Building and running
********************

.. code-block:: console

    west build -b nrf54lm20dk/nrf54lm20a/cpuapp samples/nrf_audio_eb -- -DSHIELD=nrf_audio_eb

Testing
=======

After programming the sample, complete the following steps:

1. Connect to the kit with a terminal emulator at 115200 baud.
2. Reset the kit.
3. Observe the result lines printed for each interface.

The expected output on the nRF54LM20 DK, with an SD card and microphone present, is similar to the following:

.. code-block:: console

    ========================================
     nRF-Audio-EB shield smoke test
    ========================================
    [LED]  RGB blink
      red   : on
      green : on
      blue  : on
    [LINE] line-in-detect
      logical level = 0 (deasserted)
    [I2C]  TAC5112 probe @0x50
      ACK: codec present (page reg = 0x00)
    [PDM]  microphone capture
      captured 3200 bytes, peak amplitude = 0
    [I2S]  audio serial port init + TX
      codec configured: I2S target, 48000 Hz, 16-bit, 2 ch
      output enabled
      streaming 1000 Hz tone for ~1 s
      output disabled
   [SD]   mount /SD: (soft eMMC, 4-bit)
     mounted: 30432768 KB total, 30432000 KB free
     wrote 36 bytes to /SD:/eb_test.txt
     read-back OK (36 bytes): "nRF-Audio-EB sEMMC 4-bit round-trip"
    ===== smoke test complete =====

Dependencies
************

This sample uses the following Zephyr APIs and subsystems:

* :ref:`GPIO <zephyr:gpio_api>`
* :ref:`I2C <zephyr:i2c_api>`
* :ref:`I2S <zephyr:i2s_api>`
* The DMIC (PDM) audio API (``include/zephyr/audio/dmic.h``)
* :ref:`File system <zephyr:file_system_api>` with FAT and the SDMMC disk driver, on the soft eMMC (sEMMC) SDHC controller (:dtcompatible:`nordic,nrf-semmc`)
