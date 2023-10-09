.. _lib_audio_module:

Audio module
############

.. contents::
   :local:
   :depth: 2

.. note::
   Use this template to create pages that describe the libraries included in the |NCS|, following these instructions:

   * Use this first section to give a short description (1 or 2 sentences) of the library, indicating what it does and why it should be used.

   * Give the library a concise name that also corresponds to the folder name.

     If the library targets a specific protocol or device, add it in the title before the library name (for example, "NFC:").
     Do not include the word "library" in the title.
     Use the provided stock phrases and includes when possible.

   * Sections with ``*`` are optional and can be left out.
     All the other sections are required for all libraries.

     Do not add new sections or remove a mandatory section without consulting a tech writer.
     If any new section is needed, try first to fit it as a subsection of one of the sections listed below.

     When you keep an optional section, remove the asterisk from the title, and format the length of the related RST heading accordingly.


The audio module library coordinates audio states and the exchange of audio-related data of an LE Audio application, such as :ref:`nrf5340_audio`.

Overview
********

.. note::
   Use this section to give a general overview of the library.

Implementation*
===============

.. note::
   Use this section to describe the library architecture and implementation.

Supported features*
===================

.. note::
   Use this section to describe the features supported by the library.

Supported backends*
===================

.. note::
   Use this section to describe the backends supported by the library, if applicable.

Requirements*
*************

.. note::
   Use this section to list the requirements needed to use the library.
   It is a mandatory section if there are specific requirements that must be met to use the library.

This library can only be used with an LE Audio application, for example :ref:`nrf5340_audio`.

Configuration
*************

To use the audio module library, enable the :kconfig:option:`AUDIO_MODULE` Kconfig option.


Usage
*****

.. note::
   Use this section to explain how to use the library.
   This is optional if the library is so small that the initial short description also provides information about how to use the library.

Samples using the library
*************************

The following |NCS| applications use this library:

* :ref:`nrf5340_audio`

Application integration*
************************

.. note::
   Use this section to explain how to integrate the library in a custom application.

Additional information*
***********************

.. note::
   Use this section to provide any additional information relevant to the user.

Limitations*
************

.. note::
   Use this section to describe any limitations to the library, if present.

Dependencies
************

.. note::
   Use this section to list all dependencies of this library, if applicable.

API documentation
*****************

| Header file: :file:`include/audio_module/audio_module.h`
| Source files: :file:`subsys/audio_module/audio_module.c`

.. doxygengroup:: audio_module
   :project: nrf
   :members:

