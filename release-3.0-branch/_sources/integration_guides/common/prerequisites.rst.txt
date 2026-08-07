:orphan:

.. hubble-integration-prerequisites

Prerequisites
*************

Before starting, ensure you have the following:

* **A supported development kit or custom board** with a PA or FEM capable of
  at least +20 dBm transmit output power. The Hubble satellite link budget
  requires this minimum output to reach the network reliably.
* **An antenna tuned to the Hubble satellite frequency band**, connected to
  the RF output of the PA or FEM.
* **A Hubble account and API key** to fetch ephemeris data for pass prediction.
* **ADALM-PLUTO SDR** *(optional, recommended for custom board bring-up)*:
  used to verify RF output at the physical layer. Available from common
  distributors such as DigiKey and Mouser. See the `ADALM-PLUTO product page`_
  for details.

.. _ADALM-PLUTO product page: https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html#eb-overview
