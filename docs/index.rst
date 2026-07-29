.. Hubble Device SDK documentation

.. _hubble-home:

Hubble Device SDK documentation
================================

**Welcome to the Hubble Device SDK documentation!**

This guide will walk you through everything you need to know to integrate the
Hubble APIs with your application or service and begin leveraging the Hubble
Network. Designed for developers who want a straightforward and reliable way to
harness Hubble’s powerful capabilities, this SDK provides a suite of tools and
libraries that make it easy to interact with all of Hubble’s services.

Using Hubble Device SDK
************************


.. list-table:: Platform Support
   :widths: 25 25 50
   :header-rows: 1

   * - Platform
     - Quick Start
     - Notes
   * - Zephyr
     - :ref:`zephyr_quick_start`
     - Best class support, reference implementation.
   * - FreeRTOS
     - :ref:`freertos_quick_start`
     - FreeRTOS support with user-managed build integration.
   * - TI SIMPLELINK / SysConfig
     - :ref:`ti_quick_start`
     - TI SimpleLink SDK + SysConfig integration.
   * - ESP-IDF
     - :ref:`esp_idf_quick_start`
     - ESP-IDF support with component-based integration.

Additional Resources
********************

.. raw:: html

   <ul class="grid">
       <li class="grid-item">
	   <a href="introduction/index.html">
               <span class="grid-icon fa fa-info"></span>
	       <h2>About Hubble Device SDK</h2>
	   </a>
	   <p>Overview, features &amp; where to start</p>
       </li>
       <li class="grid-item">
	   <a href="configuration/index.html">
               <span class="grid-icon fa fa-cogs"></span>
	       <h2>Configuration Reference</h2>
	   </a>
	   <p>Kconfig options for BLE, satellite, crypto &amp; more</p>
       </li>
       <li class="grid-item">
	   <a href="architecture/index.html">
               <span class="grid-icon fa fa-sitemap"></span>
	       <h2>Architecture</h2>
	   </a>
	   <p>Service modules, port layer, code organization &amp; conventions</p>
       </li>
       <li class="grid-item">
	   <a href="terrestrial/index.html">
               <span class="grid-icon fa fa-bluetooth-b"></span>
	       <h2>Terrestrial Network</h2>
	   </a>
	   <p>Terrestrial Network, build &amp; run a sample application</p>
       </li>
       <li class="grid-item">
	   <a href="satellite/index.html">
               <span class="grid-icon fa fa-wifi"></span>
	       <h2>Satellite Network (Pre-Production)</h2>
	   </a>
	   <p>Satellite Network, build &amp; run a sample application</p>
       </li>
   </ul>

.. toctree::
   :maxdepth: 1
   :hidden:

   introduction/index
   architecture/index
   terrestrial/index
   satellite/index
   quickstart/index
   integration_guides/index
   best-practices/index
   security/index
   releases/index

.. toctree::
   :maxdepth: 1
   :hidden:
   :caption: Reference

   api/index
   configuration/index

