.. _nrf_audio_eb:

nRF-Audio-EB shield
###################

.. contents::
   :local:
   :depth: 2

Overview
********

The nRF-Audio-EB shield brings up the nRF-Audio-EB audio expansion board, which carries a Texas Instruments TAC5112 stereo audio codec.
It is a multi-board shield: the SoC-specific devicetree lives in per-board overlays under :file:`boards/`, so a single shield definition serves several development kits.
The base :file:`nrf_audio_eb.overlay` is a neutral stub; |NCS| applies it for every board and then layers the matching :file:`boards/<board>.overlay` on top.

The shield enables the following interfaces (subject to what each carrier's connector exposes):

* Audio over the Zephyr I2S API, through the ``tac5112-i2s`` alias (the TDM peripheral on the nRF54LM20).
* Codec control over I2C, exposed as the ``audio_codec`` node (:dtcompatible:`ti,tac5112`).
* Digital microphone over the DMIC API, through the ``dmic0`` alias (nRF54LM20 only).
* A microSD card in 4-bit mode via the soft eMMC (sEMMC) soft peripheral running on the FLPR (VPR) core, using the SDHC subsystem with :dtcompatible:`nordic,nrf-semmc` (nRF54LM20 only).
* An RGB LED (:dtcompatible:`gpio-leds`) and a line-in-detect input (:dtcompatible:`gpio-keys`) (nRF54LM20 only).

Requirements
************

This shield supports the following development kits.

.. list-table:: Supported carriers
   :header-rows: 1
   :widths: 30 35 35

   * - Development kit
     - Board target
     - Connector
   * - nRF54LM20 DK
     - ``nrf54lm20dk/nrf54lm20a/cpuapp``
     - P17 (2x13 header, mates 1:1)

Audio codec
===========

|NCS| and Zephyr do not provide an in-tree driver for the TAC5112.
The shield ships two bus bindings for the same ``compatible = "ti,tac5112"`` value, and the build system selects the one that matches the parent bus of the ``audio_codec`` node:

* :file:`dts/bindings/audio/ti,tac5112.yaml` for I2C control (used by all three carriers above).

The application configures the codec registers itself.
Obtain the bus with ``I2C_DT_SPEC_GET(DT_NODELABEL(audio_codec))`` (or ``SPI_DT_SPEC_GET`` for SPI control) and the side-band lines with ``GPIO_DT_SPEC_GET``.
The 7-bit I2C address is ``0x50`` when the ADDR pin is strapped to ground; the other straps give ``0x51``, ``0x52`` or ``0x53``.
The codec runs in PLL mode, recovering its clocks from BCLK, so MCLK is routed but is not used for clocking.

Pin assignments
***************

nRF54LM20 DK (P17)
==================

The nRF-Audio-EB P2 header (2x13 socket) mates 1:1 with the nRF54LM20 DK P17 header.
The power pins align in both (P17 VDD on 1 and 20, GND on 2 and 19), which confirms that P17 pin *N* connects to P2 pin *N*.
All rows below are confirmed against both pinout tables.

.. list-table:: nRF54LM20 DK P17 to nRF-Audio-EB
   :header-rows: 1
   :widths: 12 22 30 36

   * - Pin
     - nRF54LM20 GPIO
     - nRF-Audio-EB signal
     - Shield use
   * - 5
     - P1.00
     - TDM_FSYNC
     - I2S LRCK
   * - 6
     - P1.03
     - TDM_SCK
     - I2S BCLK (dedicated clock pin)
   * - 7
     - P1.04
     - TDM_MCK
     - I2S MCK
   * - 8
     - P1.13
     - TDM_SDOUT
     - I2S SDOUT (SoC to codec)
   * - 11
     - P0.03
     - PDM_CLK
     - PDM clock
   * - 12
     - P0.04
     - PDM_DAT
     - PDM data
   * - 15
     - P1.07
     - SCL
     - I2C SCL (``i2c22``)
   * - 16
     - P1.06
     - SDA
     - I2C SDA (``i2c22``)
   * - 17
     - P1.05
     - TDM_SDIN
     - I2S SDIN (codec to SoC)
   * - 18
     - P3.06
     - LINE_IN_DETECT
     - Line-in-detect input
   * - 21 - 26
     - P2.00 - P2.05
     - QSPI (microSD)
     - Soft eMMC bus (``gpio2``); roles per the fixed pin map above

Audio uses the TDM peripheral (the nRF54LM20 has no I2S peripheral), driven through the standard I2S API.

The codec interrupt (``fault-gpios``) is on P3.05 and the RGB LED on P3.01 / P3.03 / P3.00 (red / green / blue); together with the line-in-detect input on P3.06, these are all on ``gpio3``, per the board overlay.

The microSD card is driven by the **soft eMMC (sEMMC)** soft peripheral, which emulates a 4-bit SD/eMMC host on the FLPR (VPR) core.
It is loaded and driven from the application core by the in-tree Zephyr SDHC driver (:kconfig:option:`CONFIG_SDHC_NRF_SEMMC`, :dtcompatible:`nordic,nrf-semmc`), which plugs into the standard SDHC → SD subsystem → Disk Access stack, so the card is still exposed as disk ``"SD"``.
The six SD lines are on GPIO **port P2** (``gpio2``); the ``cpuflpr_vpr`` node is enabled and a shared-RAM window for the soft-peripheral register interface is reserved under ``reserved-memory``.

The sEMMC FLPR firmware uses a **fixed** pin map that is baked into the firmware blob (it is *not* selectable through pinctrl):

.. list-table:: sEMMC fixed pin map (nRF54L series)
   :header-rows: 1
   :widths: 30 30

   * - nRF54LM20 pin
     - SD signal
   * - P2.00
     - DAT3
   * - P2.01
     - CLK
   * - P2.02
     - DAT0
   * - P2.03
     - DAT2
   * - P2.04
     - DAT1
   * - P2.05
     - CMD

.. important::
   Because the pin map is fixed in firmware, the carrier board must route the
   microSD card lines to exactly these pins. The nRF-Audio-EB revision that ships
   with the SPI SD routing does **not** match this map; a board revision that
   re-routes the microSD bus to the pins above is required for sEMMC to function.

Usage
*****

Set ``SHIELD`` to ``nrf_audio_eb`` when building an application.
Because the shield and the codec bindings are hosted in this out-of-tree repository, point the build at the repository root with both ``BOARD_ROOT`` (for the shield) and ``DTS_ROOT`` (for the ``ti,tac5112`` bindings).

.. code-block:: console

   west build -b nrf54lm20dk/nrf54lm20a/cpuapp -- \
       -DBOARD_ROOT=/home/grwa/shield -DDTS_ROOT=/home/grwa/shield -DSHIELD=nrf_audio_eb

.. note::
   The microSD card runs in 4-bit mode through the soft eMMC (sEMMC) soft
   peripheral on the FLPR core (the nRF54LM20 has no hardware SDHC controller).
   sEMMC requires a board revision whose microSD lines are routed to the FLPR
   firmware's fixed port-2 pin map (see `Pin assignments`_).

References
**********

- `TAC5112 product page <https://www.ti.com/product/TAC5112>`_
