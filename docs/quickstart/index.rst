.. _hubble_quickstart:

Platform Setup
##############

These guides cover **getting the Hubble Device SDK into your build system** and
running a first sample application on each supported platform. Pick the guide
that matches your environment.

Each guide shows how to pull in the SDK, enable the BLE (Terrestrial) and/or
Satellite Network modules, and build and flash a sample.

.. note::

   Platform Setup is the starting point. Once the SDK builds on your platform,
   the :ref:`hubble_integration_guides` walk through building a complete
   satellite dual-stack application end to end (pass prediction, BLE
   provisioning, RF verification) on supported vendor hardware.

.. toctree::
   :maxdepth: 1
   :glob:

   zephyr/index
   freertos/index
   ti/index
   esp-idf/index
   bare-metal/index
