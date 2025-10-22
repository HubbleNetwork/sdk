# Hubble SDK — Python

A small, typed Python library and CLI for host‑side interactions with the Hubble SDK. It provides:

* **Stateless utilities**: `scan()` (BLE), `ingest()` (push encrypted packets to the cloud)
* **Domain objects**: `Organization`, `Device`, `EncryptedPacket`, `DecryptedPacket`, `Location`
* **CLI** (optional): `hubble` command that wraps common flows

> This README targets the Python subtree (e.g., `sdk/python/`). If you’re looking for firmware/C/Zephyr docs, see the repo root.

---

## Installation

### Requirements

* Python **3.9+** (3.11/3.12 recommended)
* Platform prerequisites if you use BLE scanning (`scan()`):

  * macOS: CoreBluetooth (built‑in); run in a regular user session
  * Linux: BlueZ; user must have permission to access BLE (often `bluetooth` group)
  * Windows: requires a compatible BLE stack

### Users (stable release)

```bash
pip install hubble-sdk
# Or install CLI in an isolated venv:
pipx install hubble-sdk
```

### Developers (editable install)

From the repo root:

```bash
cd python
python3 -m venv .venv && source .venv/bin/activate
pip install -e .[dev]
```

---

## Quick start

### Scan locally, then ingest to backend

```python
from hubble import ble, Organization

org = Organization(org_id="org_123", api_token="<token>")
pkt = ble.scan(timeout=5.0)
if pkt is not None:
    org.ingest_packet(pkt)
```

### Manage devices and query packets (Organization API)

```python
from hubble import Organization

org = Organization(org_id="org_123", api_token="<token>")

# Create a new device
new_dev = org.register_device(name="field-node-01")
print("new device id:", new_dev.id)

# List devices
for d in org.list_devices():
    print(d.id, d.name)

# Get packets from a device
packets = org.get_packets(new_dev)
if latest:
    print("latest RSSI:", packets[0].rssi, "payload bytes:", len(packets[0].payload))
```

### Local decryption (when you have the key)

```python
from hubble import Device, ble, decrypt

# Device with a provisioned key
dev = Device(id="dev_abc", key=b"<secret-key>")

pkts = ble.scan(timeout=5.0)
if len(pkts) > 0:
    pkt = pkts[0]
    maybe_dec = decrypt(dev.key, pkt)
    if maybe_dec:
        print("payload:", maybe_dec.payload)
```

---

## CLI usage (optional)

If installed (via `pipx install`), a `hubble` command is available.

```bash
hubble --help
hubble ble scan
```

---

## Configuration

Some functions (e.g., `org.ingest_packet`) may read defaults from environment variables if not provided explicitly. Suggested variables:

* `HUBBLE_ORG_ID` – default organization id
* `HUBBLE_API_TOKEN` – API token

You can also provide an explicit `Organization(org_id, api_token)` and pass objects directly where supported.

---

## Public API (summary)

Import from the top‑level package for a stable surface:

```python
from hubble import (
    ble, cloud,
    Organization, Device,
    EncryptedPacket, DecryptedPacket, Location,
)
```

### Stateless functions

* `ble.scan(timeout: float) -> Optional[EncryptedPacket]` — Listen via BLE and return the first packet observed, or `None` on timeout.

### Classes

* `Organization(org_id: str, api_token: str)`

  * `register_device(name: Optional[str] = None) -> Device`
  * `list_devices() -> list[Device]`
  * `get_packets(device: Device | str) -> Optional[DecryptedPacket]`
* `Device(id: str, key: Optional[bytes] = None, name: Optional[str] = None)`
* `EncryptedPacket(timestamp, location, payload: bytes, rssi: int)`
* `DecryptedPacket(timestamp, location, tags: list[str], payload: bytes, rssi: int, counter: Optional[int], sequence: Optional[int])`
* `Location(lat: float, lon: float, alt_m: Optional[float])`

---

## Project layout

```
src/hubble/
├─ __init__.py       # public API façade (re-exports)
├─ packets.py        # data models: packets, location, ids
├─ device.py         # Device class: local decrypt
├─ crypto.py         # Crypto functionality
├─ org.py            # Organization class: backend ops
├─ ble.py            # scan() BLE discovery
├─ cloud.py          # cloud functionality
├─ errors.py         # exceptions
└─ cli/__init__.py   # CLI entry: main()
```

---

## Development

Run tests and linters from the `python/` directory:

```bash
pytest -q
ruff check src
mypy src
```

Recommended editor settings:

* Enable `ruff` and `mypy`
* 100‑column soft wrap
* `python.analysis.typeCheckingMode = basic/strict` (VS Code)

---

## Versioning & releases

* Follows **SemVer** (MAJOR.MINOR.PATCH)
* Tagged releases (e.g., `v0.1.0`) publish wheels/sdists to PyPI
* CLI entry point: `hubble = "hubble.cli:main"` (stable)

---

# Testing

This package uses **pytest** with a small set of markers to separate fast unit tests from hardware/OS-dependent integration tests (e.g., BLE).

## Test layout

```
python/
├─ src/hubble/...
└─ tests/
   ├─ unit/            # fast, hermetic tests
   └─ integration/     # slower or environment-dependent tests
      └─ test_ble_scan.py
```

Markers (declared in `pyproject.toml`):

* `integration` – slow or environment-dependent tests
* `ble` – tests that require BLE hardware/permissions

## Setup (one-time per venv)

```bash
cd python
python3 -m venv .venv
source .venv/bin/activate

# Dev tools only:
pip install -e '.[dev]'
```

## Running tests

### All fast unit tests

```bash
pytest -q tests/unit
```

### Only integration tests

```bash
pytest -q -m integration
```

### BLE scan integration test (opt-in)

BLE tests are disabled by default to avoid flakiness on CI or machines without radio permissions. Enable explicitly:

```bash
export HUBBLE_BLE_TEST=1         # opt-in gate used by the test
pytest -q -m ble                 # run only BLE-marked tests
# or target a single file:
pytest -q tests/integration/test_ble_scan.py
```

You can adjust the scan timeout:

```bash
HUBBLE_BLE_TIMEOUT=8 pytest -q -m ble
```

### Coverage (optional)

```bash
pytest --cov=hubble --cov-report=term-missing
```

### Test Troubleshooting

* **`zsh: no matches found: .[dev]`**
  Quote the extras spec (zsh globs brackets):
  `pip install -e '.[dev]'` or `pip install -e '.[dev,ble]'`.

* **BLE on macOS/Linux/Windows**

  * macOS: works in a user session (CoreBluetooth).
  * Linux: ensure BlueZ is installed and your user has adapter permissions.
  * Windows: requires a compatible BLE adapter; run in a standard desktop session.

* **Running from repo root**
  Either change into `python/` before running `pytest`, or pass the path:
  `pytest -q python/tests`

---

## General Troubleshooting

* **`ble.scan()` finds nothing**: verify BLE permissions and adapter state; try increasing `timeout`.
* **Auth errors**: confirm `Organization(org_id, api_token)` or env vars are set; check token scope/expiry.
* **Import errors**: ensure you installed into the Python you’re running (`python -m pip …`). Prefer `pipx` for CLI‑only usage.

---

## Contributing

* Open issues/PRs with clear repro steps
* Keep modules cohesive; avoid mixing concerns
* Add/extend unit tests for new behavior

---

## License

This project is licensed under the **MIT License**. See `LICENSE` for details.

---

## Security / responsible disclosure

If you believe you’ve found a security issue, please email [security@hubble.network](mailto:security@hubble.network) with details and a way to reproduce. We’ll acknowledge receipt and follow up promptly.
