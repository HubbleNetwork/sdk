import os
import pytest

# Only run if the BLE extra is installed
pytest.importorskip("bleak", reason="bleak not installed (install with hubble-sdk)")

# Import the namespaced BLE module from your package
from hubble import ble


@pytest.mark.integration
@pytest.mark.ble
def test_ble_scan_returns_packet():
    """
    Integration test: runs a real BLE scan and asserts we see at least one packet.

    This test is intentionally opt-in to avoid flakiness on CI or machines without BLE.
    Enable by setting HUBBLE_BLE_TEST=1 in the environment.
    """
    if not os.getenv("HUBBLE_BLE_TEST"):
        pytest.skip("Set HUBBLE_BLE_TEST=1 to enable BLE scan integration test")

    # Allow the timeout to be configured via env; default to 5s
    timeout = float(os.getenv("HUBBLE_BLE_TIMEOUT", "5"))

    pkt = ble.scan(timeout=timeout)
    assert pkt is not None, f"No BLE packet found within {timeout:.2f}s"
