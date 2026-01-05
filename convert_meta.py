import sys
import struct
import json

# -----------------------------
# Binary layout (little-endian)
# -----------------------------

# Version string (char[10], zero-padded)
VERSION_SIZE = 10
SUPPORTED_VERSION = "0_1_0"

META_HEADER_FMT = "<I"  # meta_count

# Meta entry format (flags are uint32)
# NOTE: Color is assumed to be 4 bytes (RGBA: 4 * uint8)
META_ENTRY_FMT = "<I4B4B4B5II"

META_HEADER_SIZE = struct.calcsize(META_HEADER_FMT)
META_ENTRY_SIZE = struct.calcsize(META_ENTRY_FMT)


def _read_version(data: bytes) -> str:
    if len(data) < VERSION_SIZE:
        raise ValueError("Invalid meta file (too small for version)")

    raw = data[:VERSION_SIZE]
    # C writes a char[10] buffer that is zero-padded
    ver = raw.split(b"\x00", 1)[0].decode("ascii", errors="strict")
    return ver


def _pack_version(version: str) -> bytes:
    b = version.encode("ascii", errors="strict")
    if len(b) >= VERSION_SIZE:
        # In C you have char[10] and they copy VERSION; must fit with '\0'
        raise ValueError(f'Version "{version}" too long for {VERSION_SIZE} bytes')
    return b + b"\x00" * (VERSION_SIZE - len(b))


def bin_to_json(data: bytes) -> str:
    offset = 0

    # READ THE VERSION
    version = _read_version(data)
    offset += VERSION_SIZE

    if version != SUPPORTED_VERSION:
        raise ValueError(f'Unsupported meta version "{version}" (expected "{SUPPORTED_VERSION}")')

    if len(data) < offset + META_HEADER_SIZE:
        raise ValueError("Invalid meta file (too small)")

    (meta_count,) = struct.unpack_from(META_HEADER_FMT, data, offset)
    offset += META_HEADER_SIZE

    expected_size = VERSION_SIZE + META_HEADER_SIZE + meta_count * META_ENTRY_SIZE
    if len(data) != expected_size:
        raise ValueError(
            f"Invalid meta file size. Got {len(data)} bytes, expected {expected_size}."
        )

    metas = []
    for _ in range(meta_count):
        unpacked = struct.unpack_from(META_ENTRY_FMT, data, offset)
        offset += META_ENTRY_SIZE

        meta = {
            "tile_hash": unpacked[0],
            "bg_color":  list(unpacked[1:5]),
            "win_color": list(unpacked[5:9]),
            "obj_color": list(unpacked[9:13]),
            "bg_for_z":     unpacked[13],
            "bg_back_z":    unpacked[14],
            "win_z":        unpacked[15],
            "obj_z":        unpacked[16],
            "obj_behind_z": unpacked[17],
            "flags":        unpacked[18],
        }

        metas.append(meta)

    return json.dumps(metas, indent=2)


def _require_color(name, v):
    if not isinstance(v, (list, tuple)) or len(v) != 4:
        raise ValueError(f'"{name}" must be a list of 4 integers (0..255)')

    out = []
    for x in v:
        if not isinstance(x, int) or not (0 <= x <= 255):
            raise ValueError(f'"{name}" values must be ints in 0..255')
        out.append(x)

    return out


def json_to_bin(text: str) -> bytes:
    metas = json.loads(text)
    if not isinstance(metas, list):
        raise ValueError("JSON must be a list of meta objects")

    out = bytearray()

    # STORE THE VERSION
    out += _pack_version(SUPPORTED_VERSION)

    # STORE THE META COUNT
    out += struct.pack(META_HEADER_FMT, len(metas))

    for m in metas:
        bg = _require_color("bg_color", m["bg_color"])
        win = _require_color("win_color", m["win_color"])
        obj = _require_color("obj_color", m["obj_color"])

        flags = int(m.get("flags", 0))
        if not (0 <= flags <= 0xFFFFFFFF):
            raise ValueError('"flags" must be a uint32 (0..4294967295)')

        out += struct.pack(
            META_ENTRY_FMT,
            int(m["tile_hash"]),
            *bg,
            *win,
            *obj,
            int(m["bg_for_z"]),
            int(m["bg_back_z"]),
            int(m["win_z"]),
            int(m["obj_z"]),
            int(m["obj_behind_z"]),
            flags,
        )

    return bytes(out)


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("bin2json", "json2bin"):
        print("usage: meta_convert.py [bin2json|json2bin]", file=sys.stderr)
        sys.exit(1)

    mode = sys.argv[1]

    if mode == "bin2json":
        data = sys.stdin.buffer.read()
        sys.stdout.write(bin_to_json(data))
    else:
        text = sys.stdin.read()
        sys.stdout.buffer.write(json_to_bin(text))


if __name__ == "__main__":
    main()