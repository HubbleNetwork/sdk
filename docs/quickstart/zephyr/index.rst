.. _zephyr_quick_start:

Zephyr Quick Start
==================

Hubble Network can be added to the manifest of an existing Zephyr project or
function as the manifest repository. The following sections describe each
approach.

Adding Hubble Network to Zephyr
*******************************

This procedure explains how to add Hubble Network to an existing Zephyr
project or to start a new project with Zephyr as the manifest repository. For
new projects, refer to the `Zephyr Getting Started Guide <https://docs
.zephyrproject.org/4.3.0/getting_started/index.html>`_ to set up the
environment.

After creating or selecting a Zephyr project, integrate Hubble Network by
pulling the SDK into the Zephyr workspace. Adjust a West sub-manifest (or
create a new one) by including the snippet below:

.. code-block:: yaml

   manifest:
     remotes:
       - name: hubble
         url-base: https://github.com/HubbleNetwork
     projects:
       - name: hubblenetwork-sdk
         repo-path: hubble-device-sdk
         revision: main
         path: modules/lib/hubblenetwork-sdk
         remote: hubble
         groups:
           - optional

Then run:

.. code-block:: bash

   west update hubblenetwork-sdk

To enable the required modules, add the corresponding lines to the project’s
``prj.conf`` file:

.. code-block::

    CONFIG_HUBBLE_BLE_NETWORK=y
    CONFIG_HUBBLE_SAT_NETWORK=y

Use ``CONFIG_HUBBLE_BLE_NETWORK=y`` to enable the Bluetooth® Low Energy (BLE) Network module or
``CONFIG_HUBBLE_SAT_NETWORK=y`` for the Satellite Network module.

Using Hubble Network as the manifest repository
***********************************************

Hubble Network can also serve as the manifest repository. In this case,
install West and all required `Zephyr dependencies
<https://docs.zephyrproject.org/4.3.0/develop/getting_started/index
.html#install-dependencies>`_ as described in the `Zephyr documentation <https://docs.zephyrproject
.org/4.3.0/develop/toolchains/zephyr_sdk.html#zephyr-sdk-installation>`_. The
following steps outline the process of creating a Zephyr workspace that uses the Hubble Network SDK as the manifest repository:


.. _Python virtual environment: https://docs.python.org/3/library/venv.html

      #. Create a Python virtual environment:

         .. code-block:: bash

            python3 -m venv ~/hubblenetwork-workspace/.venv

      #. Activate the virtual environment:

         .. code-block:: bash

            source ~/hubblenetwork-workspace/.venv/bin/activate

         Once activated your shell will be prefixed with ``(.venv)``.
         Deactivate at any time by running ``deactivate``. Re-activation is
         necessary whenever returning to work in this environment.

      #. Install west:

         .. code-block:: bash

            pip install west

      #. Obtain the Zephyr source code:

         .. code-block:: bash

            west init -m git@github.com:HubbleNetwork/hubble-device-sdk.git ~/hubblenetwork-workspace
            cd ~/hubblenetwork-workspace/modules/lib/hubblenetwork-sdk
            west update

      #. Export a Zephyr:

         .. code-block:: bash

            west zephyr-export

         Exports a Zephyr CMake package, allowing CMake to automatically load
         the boilerplate code needed for building Zephyr applications.

      #. Install Python dependencies:

         .. code-block:: bash

            west packages pip --install
            pip install -r ~/hubblenetwork-workspace/zephyr/scripts/requirements.txt



Building and running your first application
+++++++++++++++++++++++++++++++++++++++++++

Building and Running the First Application Once the steps in the previous
section are complete, the system is ready to use Hubble Network. The following
commands demonstrate how to build and flash a BLE Network application.

Build the application
---------------------


.. code-block:: bash

   cd ~/hubblenetwork-workspace/
   west build -p -b nrf52840dk/nrf52840 modules/lib/hubblenetwork-sdk/samples/zephyr/ble-network

Flash the application
---------------------

.. code-block:: bash

   west flash
