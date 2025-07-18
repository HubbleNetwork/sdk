# Hubble Network BLE Sample Application

## Overview

This sample application demonstrates how to use the Hubble Network SDK on a
Zephyr-based device to broadcast data over Bluetooth Low Energy (BLE).

The application initializes the Hubble SDK, sets an encryption key and a UTC
timestamp, and then advertises user-provided data within a BLE packet. It
provides a shell interface over the serial port to input the necessary
configuration and data.

## Requirements

- A serial terminal program (e.g., `minicom`, `screen`, PuTTY).
- A tool for sending raw binary data over serial, such as `xxd` and `dd`.
- Cryptographic key provided by Hubble Network

## Building and Flashing

1.  Open a terminal in the project directory (`samples/zephyr/ble-network`).
2.  Build the application using `west`, replacing `<your_board_here>` with your
    target board's identifier (e.g., `nrf52840dk/nrf52840`).

    ```sh
    west build -b <your_board_here>
    ```

3.  Flash the application to your board:

    ```sh
    west flash
    ```

## Running the Sample

After flashing, connect to your device's serial port using a terminal emulator
(e.g., at 115200 baud).

You will see a prompt like this:
```
Insert key and utc time to start. Type help for more information.
uart:~$
```

The application will not start advertising until both the encryption key and
the UTC time are set.

### Step 1: Set the Encryption Key

The application expects an encryption key to be sent as raw
binary data over the serial port.


1.  In the sample's shell, type the `key` command and press Enter. The device
    will now wait to receive the raw key data.

    ```
    uart:~$ key
    Please transmit the key through the serial. e.g: xxd -p key > /dev/ttyX
    Loading...
    press ctrl-x ctrl-q to escape
    ```

 ---
**NOTE**

The key can be provisioned at build time using `provisioning-key.py` tool. 

---

2.  From a separate terminal on your host machine, use a command to send the
    raw binary representation of your key to the board's serial port (e.g.,
    `/dev/ttyACM0`).

    ```sh
    xxd -r -p key.hex | dd of=/dev/ttyACM0 bs=1
    ```
    *   `xxd -r -p` converts the hex string back into binary.
    *   `dd` writes the data to the serial port byte-by-byte.

3.  After sending the key, you must send an escape sequence to signal the end
    of the transmission. Press `Ctrl+X` followed by `Ctrl+Q` in your serial
    terminal.

    You should see a confirmation that the key has been set.

### Step 2: Set the UTC Time

Set the current UTC time using the `utc` command, providing the time as a Unix
timestamp (miliseconds since the epoch).

You can get the current Unix timestamp with the following command:
```sh
python -c "import time;print(int(time.time() * 1000))"
# or using coreutils
date +"%s%3N"
```

In the sample's shell, run the `utc` command with the timestamp:
```sh
uart:~$ utc 1752865580290
```

Once the UTC time is set, the application will initialize the Hubble SDK and
the Bluetooth subsystem.

### Step 3: Advertise Data

Now you can provide the data you want to broadcast using the `data` command.

```sh
uart:~$ data "Hello Hubble"
```

The application will take this string, process it with the Hubble SDK, and
place the resulting data in the service data field of its BLE advertisement
packet.

## Shell Commands

This sample adds the following custom commands to the Zephyr shell:

| Command | Description                                     | Arguments                            |
| :------ | :---------------------------------------------- | :------------------------------------|
| `key`   | Puts the device in key-loading mode.            | None                                 |
| `utc`   | Sets the UTC time.                              | `<timestamp>` (Unix timestamp in ms) |
| `data`  | Sets the data payload to be advertised.         | `<string>` (e.g., "your data")       |
