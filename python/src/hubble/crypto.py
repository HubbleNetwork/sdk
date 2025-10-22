from __future__ import annotations
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from Crypto.Protocol.KDF import SP800_108_Counter
from datetime import datetime, timezone

from .packets import EncryptedPacket, DecryptedPacket

# Valid values are 16 and 32, respectively for AES-128 and AES-256
_HUBBLE_AES_KEY_SIZE = 32

_HUBBLE_AES_NONCE_SIZE = 12
_HUBBLE_AES_TAG_SIZE = 4


def _generate_kdf_key(key: bytes, key_size: int, label: str, context: int) -> bytes:
    label = label.encode()
    context = str(context).encode()

    return SP800_108_Counter(
        key,
        key_size,
        lambda session_key, data: CMAC.new(session_key, data, AES).digest(),
        label=label,
        context=context,
    )


def _get_nonce(key: bytes, time_counter: int, counter: int) -> bytes:
    nonce_key = _generate_kdf_key(key, _HUBBLE_AES_KEY_SIZE, "NonceKey", time_counter)

    return _generate_kdf_key(nonce_key, _HUBBLE_AES_NONCE_SIZE, "Nonce", counter)


def _get_encryption_key(key: bytes, time_counter: int, counter: int) -> bytes:
    encryption_key = _generate_kdf_key(
        key, _HUBBLE_AES_KEY_SIZE, "EncryptionKey", time_counter
    )

    return _generate_kdf_key(encryption_key, _HUBBLE_AES_KEY_SIZE, "Key", counter)


def _get_auth_tag(key: bytes, ciphertext: bytes) -> bytes:
    computed_cmac = CMAC.new(key, ciphertext, AES).digest()

    return computed_cmac[:_HUBBLE_AES_TAG_SIZE]


def _aes_decrypt(key: bytes, session_nonce: bytes, ciphertext: bytes) -> bytes:
    cipher = AES.new(key, AES.MODE_CTR, nonce=session_nonce)

    return cipher.decrypt(ciphertext)


def decrypt(
    key: bytes, encrypted_pkt: EncryptedPacket, days: int = 2
) -> Optional[DecryptedPacket]:
    ble_adv = encrypted_pkt.payload
    seq_no = int.from_bytes(ble_adv[0:2], "big") & 0x3FF
    device_id = ble_adv[2:6].hex()
    auth_tag = ble_adv[6:10]
    encrypted_payload = ble_adv[10:]
    day_offset = 0

    time_counter = int(datetime.now(timezone.utc).timestamp()) // 86400

    for t in range(-days, days + 1):
        daily_key = _get_encryption_key(key, time_counter + t, seq_no)
        tag = _get_auth_tag(daily_key, encrypted_payload)

        if tag == auth_tag:
            day_offset = t
            nonce = _get_nonce(key, time_counter, seq_no)
            decrypted_payload = _aes_decrypt(daily_key, nonce, encrypted_payload)
            return DecryptedPacket(
                timestamp=encrypted_pkt.timestamp,
                device_id="",
                device_name="",
                location=encrypted_pkt.location,
                tags=[],
                payload=decrypted_payload,
                rssi=encrypted_pkt.rssi,
                counter=None,
                sequence=None,
            )
    return None
