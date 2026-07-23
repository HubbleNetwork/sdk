:orphan:

.. hubble-integration-next-steps

Next Steps
**********

RF Verification with an SDR
============================

Before waiting for a live satellite pass, you can verify that your device is
transmitting a valid Hubble packet at the physical layer using an
`ADALM-PLUTO product page`_ and the `pyhubblenetwork`_ Python library.

Install the library:

.. code-block:: bash

   pip install pyhubblenetwork

Connect the ADALM-PLUTO near the device antenna and run the scanner with your
device key to decode captured packets in real time:

.. code-block:: bash

   hubblenetwork sat scan --key "<your-device-key>"

A successfully decoded packet confirms the RF output, packet framing, channel
hopping sequence, and PA/FEM sequencing are all correct. For the full list of
available commands, run:

.. code-block:: bash

   hubblenetwork --help

See the `pyhubblenetwork`_ repository for detailed usage and setup
instructions.

Viewing Upcoming Passes
========================

Use the `Hubble Pass Explorer`_ to see when the next satellite pass is predicted
for your location. This is useful to cross-check pass prediction results from the
device and to plan test windows.

Dashboard Verification
======================

Once a live satellite pass has occurred and :c:func:`hubble_sat_packet_send`
returned without error, after the downlink data is successful, log into the
`Hubble Dashboard Devices Page`_ to confirm the packet was received by the network. A packet
appearing on the dashboard is end-to-end proof that the device is operational
on the Hubble satellite network.

.. _pyhubblenetwork: https://github.com/HubbleNetwork/pyhubblenetwork
.. _ADALM-PLUTO product page: https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html#eb-overview
.. _Hubble Pass Explorer: https://dash.hubble.com/satellite/pass-explorer
.. _Hubble Dashboard Devices Page: https://dash.hubble.com/devices
