.. _hubble_integration_guides:

Integration Guides
##################

End-to-end guides for building a complete **satellite dual-stack** application
(Satellite + BLE) on a specific vendor platform. Each guide covers obtaining
ephemeris data, using pass prediction to schedule transmissions, beaconing over
BLE while waiting for a pass, transmitting during the pass window, and verifying
the result on hardware.

These guides assume the SDK already builds on your platform. If you have not set
that up yet, start with :ref:`hubble_quickstart`, which covers pulling in the
SDK and running a first sample. For the concepts behind the workflow — pass
prediction, reliability modes, and clock drift — see the
:ref:`hubble_satellite_introduction`.

.. toctree::
   :maxdepth: 1

   ncs/index
   ti/index
