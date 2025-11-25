.. _hubble_ble_security:

BLE  Security Overview
######################

The cryptographic techniques used in the Hubble Bluetooth® Low Energy (BLE) Network ensure
the confidentiality, integrity, and authenticity of the data being advertised
over BLE. AES-256 provides strong encryption, CMAC ensures data integrity and
authenticity, and the KBKDF securely derives keys from a master key. The use
of nonces and sequence numbers prevents replay attacks and ensures that each
advertisement is unique.

Key concepts
************

* **AES-256 Encryption**

  The code uses AES-256 (Advanced Encryption Standard with a 256-bit key) for
  encryption. AES is a symmetric encryption algorithm, meaning the same key is
  used for both encryption and decryption.

* **CMAC (Cipher-based Message Authentication Code)**

  CMAC is used to generate a message authentication code (MAC) to ensure data
  integrity and authenticity. The CMAC is computed over the input data using the
  provided key, and the result is stored in the output buffer.

.. _hubble_ble_kbdf:

* **KBKDF (Key-Based Key Derivation Function)**

  The code uses a KBKDF to derive keys from a master key. It derives keys
  based on a master key, a label, a context, and a counter. It uses CMAC as the
  pseudorandom function (PRF) to generate the derived keys.

* **Nonce Generation**

  A nonce (number used once) is generated for each encryption operation to
  ensure that the same plaintext encrypted multiple times will produce different
  ciphertexts. The nonce is derived using the KBKDF with a specific label
  ("Nonce") and the current sequence number.


Encryption steps
################


An application will get the encrypted advertisment data using
:c:func:`hubble_ble_advertise_get`. This function performs the
following steps to prepare the advertisement data:

* Derives the device ID, nonce, and encryption key using the KBKDF.
* Sets the BLE address using the derived device ID and sequence number.
* Encrypts the payload data using AES-256 in CTR mode with the derived encryption key and nonce.
* Computes the CMAC of the encrypted payload data to generate an authentication tag.
* Copies the authentication tag and encrypted data into the advertisement buffer.


Additional references
#####################

* `FIPS 197 - Advanced Encryption Standard (AES): <https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf>`_

* `NIST - Recommendation for Key Derivation Using Pseudorandom Functions <https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-108r1-upd1.pdf>`_
