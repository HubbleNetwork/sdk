from __future__ import annotations

from .device import Device
from .errors import ElfFetchError

import io
import os
import base64
import requests
import time
import tempfile
import pylink
from pathlib import Path
from typing import Dict
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection


_ELF_BASE_URL = (
    "https://raw.githubusercontent.com/HubbleNetwork/hubble-tldm/master/merge"
)

# Map boards to J-Link "device" strings (extend as needed)
_BOARD_TO_JLINK_DEVICE: Dict[str, str] = {
    "nrf52dk": "nRF52832_xxAA",
    "nrf52840dk": "nRF52840_xxAA",
    "nrf21540dk": "nRF52840_xxAA",
    "xg24_ek2703a": "EFR32MG24BxxxF1536",
    "xg22_ek4108a": "EFR32MG22CxxxF512",
}


def _compute_file_offset(sym, sec) -> int:
    return sec["sh_offset"] + (sym["st_value"] - sec["sh_addr"])


def _get_endianness_from_elf(buf: io.BytesIO) -> str:
    buf.seek(0)
    elf = ELFFile(buf)
    """Return 'little' or 'big' by inspecting the ELF header."""
    return "little" if elf.little_endian else "big"


def _find_symbol(elf: ELFFile, name: str):
    """Return (symbol, section) for a named symbol from .symtab or .dynsym."""
    for sec in elf.iter_sections():
        if not isinstance(sec, SymbolTableSection):
            continue
        for sym in sec.iter_symbols():
            if sym.name == name:
                shndx = sym["st_shndx"]
                if shndx == "SHN_UNDEF":
                    raise ValueError(f"Symbol '{name}' is undefined (imported).")
                if isinstance(shndx, str):
                    raise ValueError(
                        f"Symbol '{name}' has special section index {shndx}, cannot patch."
                    )
                target_sec = elf.get_section(shndx)
                if target_sec is None:
                    raise ValueError(f"Could not find section for symbol '{name}'.")
                if target_sec.name == "bss":
                    continue
                return sym, target_sec
    return None, None


def _patch_symbol(buf: io.BytesIO, data: bytes, symbol_name: str):
    buf.seek(0)
    elf = ELFFile(buf)

    # Resolve symbol
    sym, sec = _find_symbol(elf, symbol_name)
    if sym is None:
        raise ValueError(f"{symbol_name} not found in elf file")

    file_off = _compute_file_offset(sym, sec)
    sym_size = int(sym["st_size"]) or 0

    if sym_size not in (0, len(data)):
        raise ValueError(
            f"Symbol size is {sym_size} bytes, but {symbol_name} length is {len(data)}"
        )

    buf.seek(file_off)
    buf.write(data)


def patch_elf(buf: io.BytesIO, device: Device):
    _patch_symbol(buf, base64.b64decode(device.key), "master_key")

    endian = _get_endianness_from_elf(buf)
    utc_ms = int(time.time() * 1000)
    _patch_symbol(buf, utc_ms.to_bytes(8, endian, signed=False), "utc_time")


def _addr_for_segment(seg) -> int:
    """
    Choose a programming address for a PT_LOAD segment.
    Prefer physical address (p_paddr) if present, else virtual (p_vaddr).
    """
    try:
        paddr = int(seg["p_paddr"])
    except Exception:
        paddr = 0
    vaddr = int(seg["p_vaddr"])
    return paddr if paddr else vaddr


def _always_unsecure(title, msg, flags):
    # proceed with mass erase + unlock
    return pylink.enums.JLinkFlags.DLG_BUTTON_YES


def flash_elf(buf: io.BytesIO, board: str) -> None:
    """
    Flash an ELF image (held in a BytesIO) to an nRF52832_xxAA using pylink.
    Creates a temporary .elf on disk (needed by jlink.flash_file) and deletes it afterwards.

    Args:
        buf: io.BytesIO positioned anywhere (we'll rewind it).
        board: board name

    Raises:
        ImportError: pylink not installed.
        RuntimeError: on J-Link connection or flashing failure.
    """
    speed_khz = 4000
    device = _BOARD_TO_JLINK_DEVICE.get(board.strip().lower())

    # Write the buffer to a real temp file so flash_file can read it (works on all OSes)
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(delete=False, suffix=".elf") as tmp:
            tmp_path = tmp.name
            buf.seek(0)
            # Stream copy to avoid duplicating memory with getvalue()
            while True:
                chunk = buf.read(1024 * 1024)
                if not chunk:
                    break
                tmp.write(chunk)
            tmp.flush()

        # jlink will silently fail post-mandated FW update of the jlink
        # for some devices due to a security dialog which pylink ignores.
        # This unsecure_hook just makes it accept the insecurity.
        jlink = pylink.JLink(unsecure_hook=_always_unsecure)

        try:
            jlink.open()
            jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
            jlink.connect(device, speed=speed_khz)

            jlink.halt()
            jlink.flash_file(tmp_path, addr=None)  # ELF contains its own load addresses
            jlink.reset()
        except Exception as e:
            raise RuntimeError(f"Flashing failed: {e}") from e
        finally:
            try:
                if jlink.opened():
                    jlink.close()
            except Exception:
                pass
    finally:
        if tmp_path:
            try:
                os.unlink(tmp_path)
            except OSError:
                pass


def fetch_elf(board: str, timeout: float = 20.0) -> io.BytesIO:
    """
    Download the board-specific ELF from HubbleNetwork/hubble-tldm/merge and
    return it as an io.BytesIO.

    Parameters
    ----------
    board_name : str
        Board identifier (e.g. 'nrf21540dk', 'xg24_ek2703a', 'xg22_ek4108a').
    timeout : float
        Requests timeout in seconds (connect + read).

    Returns
    -------
    ELFFile
        Parsed ELFFile instance. (Holds an internal reference to the stream.)

    Raises
    ------
    ValueError
        If the board is not supported or name is malformed.
    FileNotFoundError
        If the expected ELF file does not exist in the merge directory.
    ElfFetchError
        On network, HTTP, or parsing failures.
    """
    if not isinstance(board, str) or not board.strip():
        raise ValueError("board must be a non-empty string")

    local_file = os.getenv("HUBBLE_DEMO_ELF_FILE")
    if local_file:
        return io.BytesIO(Path(local_file).read_bytes())
    else:  # pull from server
        # Give option (for development) to pull binary from elsewhere
        val = os.getenv("HUBBLE_DEMO_ELF_URL_OVERRIDE")
        if val:
            base_url = val
        else:
            base_url = _ELF_BASE_URL

        url = f"{base_url}/{board}.elf"

        try:
            resp = requests.get(url, timeout=timeout)
        except requests.RequestException as e:
            raise ElfFetchError(f"Network error fetching '{url}': {e}") from e

        if resp.status_code == 404:
            raise FileNotFoundError(
                f"ELF for board '{board}' not found at {url}. "
                "Confirm the file exists in the repo's merge/ directory."
            )
        if not resp.ok:
            raise ElfFetchError(
                f"HTTP {resp.status_code} fetching '{url}': {resp.text[:200]}"
            )

        return io.BytesIO(resp.content)
