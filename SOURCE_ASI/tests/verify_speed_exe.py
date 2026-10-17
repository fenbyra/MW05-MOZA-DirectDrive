"""Offline verification only. Never launches or changes speed.exe."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("exe", type=Path)
parser.add_argument("--out", type=Path)
args = parser.parse_args()
blob = args.exe.read_bytes()
digest = hashlib.sha256(blob).hexdigest()
assert digest == "325304a2cdd5d1d5789c09c90d249774743d1b273889b1093339ac922d56a968"
assert len(blob) == 6029312 and blob[:2] == b"MZ"
pe = struct.unpack_from("<I", blob, 0x3C)[0]
assert blob[pe:pe+4] == b"PE\0\0"
machine, section_count, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", blob, pe+4)
assert machine == 0x14C
optional = pe + 24
assert struct.unpack_from("<H", blob, optional)[0] == 0x10B
base = struct.unpack_from("<I", blob, optional+28)[0]
assert base == 0x400000
sections = []
for i in range(section_count):
    off = optional + optional_size + 40*i
    virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", blob, off+8)
    sections.append((rva, raw_size, raw_offset, virtual_size))


def read(va, size):
    rva = va - base
    for origin, raw_size, raw_offset, _ in sections:
        if origin <= rva and rva + size <= origin + raw_size:
            start = raw_offset + rva - origin
            return blob[start:start+size]
    raise AssertionError(f"VA not backed by file bytes: {va:08X}")


def u32(va):
    return struct.unpack("<I", read(va, 4))[0]


checks = {}


def code(name, va, expected):
    expected = bytes.fromhex(expected)
    assert read(va, len(expected)) == expected, name
    checks[name] = {"va": f"{va:08X}", "bytes": expected.hex(" ")}


assert u32(0x8AB720 + 4) == 0x6A0590
assert u32(0x8AA828 + 36*4) == 0x6881B0
code("ITransmission_GetGear", 0x6A0590, "8b 41 38 c3")
code("IVehicle_GetSpeed", 0x6881B0, "d9 41 70 c3")
code("ITransmission_IsReversing", 0x6A0500, "8b 51 38 33 c0 85 d2 0f 94 c0 c3")
code("PVehicle_IVehicle_at_AC", 0x687E92, "c7 86 ac 00 00 00 28 a8 8a 00")
code("InputPlayer_IInput_at_48", 0x6B67F1, "c7 46 48 70 c6 8a 00")
code("InputPlayer_IVehicle_at_7C", 0x6B6839,
     "68 e0 40 40 00 c6 44 24 1c 02 e8 a8 f1 f1 ff 89 46 7c")
code("existing_GetControls", 0x69CBB0, "8d 41 08 c3")
code("native_direction_requests_R", 0x69CF66, "8b 06 6a 00 8b ce ff 50 0c")
code("native_direction_requests_first", 0x69CFA2, "8b 16 6a 02 8b ce ff 52 0c")
code("native_reverse_pedal_swap", 0x69CFB6,
     "8b 44 24 14 8b 57 48 8d 77 48 50 8b ce ff 52 18 "
     "8b 44 24 10 8b 16 50 8b ce ff 52 1c")
code("InputPlayer_primary_vtable", 0x6B67E4, "c7 06 bc c6 8a 00")
code("native_direction_entry", 0x69CEF0, "51 57 8b f9 8b 47 7c 85 c0 0f 84 d4 00 00 00")
code("native_direction_return_8", 0x69CFD5, "c2 08 00")
code("native_fetch_calls_direction", 0x6A8D89, "e8 62 41 ff ff")
code("native_SetGas", 0x69CB90, "8b 44 24 04 89 41 1c c2 04 00")
code("native_SetBrake", 0x69CBA0, "8b 44 24 04 89 41 20 c2 04 00")
assert u32(0x8AC670+6*4) == 0x69CB90
assert u32(0x8AC670+7*4) == 0x69CBA0
header = (Path(__file__).resolve().parents[1]
          / "examples/mw05_mirror_probe/direction_core.h").read_text()
header_pairs = [
    ("kHelperRva", "kHelperEntry"), ("kHelperReturnRva", "kHelperReturn"),
    ("kFetchCallRva", "kFetchCall"), ("kReverseSwapRva", "kReverseSwap"),
    ("kSetGasRva", "kSetGasCode"), ("kSetBrakeRva", "kSetBrakeCode"),
]
for rva_name, bytes_name in header_pairs:
    rva = int(re.search(rf"\b{rva_name}\s*=\s*(0x[0-9A-Fa-f]+)u", header)[1], 16)
    raw_array = re.search(rf"\b{bytes_name}\[\]\s*=\s*\{{(.*?)\}}", header, re.S)[1]
    expected = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]+)", raw_array))
    assert read(base+rva, len(expected)) == expected, bytes_name
gate_header=(Path(__file__).resolve().parents[1]/"examples/mw05_mirror_probe/session_gate.h").read_text()
for rva_name, bytes_name in [("kBlockedPredicateRva","kBlockedPredicate"),("kRaceWriteRva","kRaceWrite")]:
    rva=int(re.search(rf"\b{rva_name}\s*=\s*(0x[0-9A-Fa-f]+)u",gate_header)[1],16)
    raw_array=re.search(rf"\b{bytes_name}\[\]\s*=\s*\{{(.*?)\}}",gate_header,re.S)[1]
    expected=bytes(int(x,16) for x in re.findall(r"0x([0-9A-Fa-f]+)",raw_array))
    assert read(base+rva,len(expected))==expected,bytes_name
    checks[bytes_name]={"va":f"{base+rva:08X}","bytes":expected.hex(" ")}
code("blocked_simulation_delta_zero",0x4D91D3,"e8 d8 56 fd ff 84 c0 75 13")
code("blocked_simulation_zero_delta_branch",0x4D91EF,"c7 47 40 00 00 00 00")
code("pause_menu_flag_transition",0x4D0805,"68 64 79 89 00 33 c0 53 b9 0f 00 00 00 bf e4 1e 91 00 55 f3 ab a2 e4 1f 91 00 e8 5c 88 fe ff")
code("simulation_flags_writer",0x4B9119,"89 15 e8 1f 91 00")
report = {
    "sha256": digest,
    "bytes": len(blob),
    "checks": checks,
    "runtime_helper_calls": "only chains existing game calls; does not synthesize a call",
    "direct_gear_writes": "none",
    "header_arrays_cross_checked": len(header_pairs)+2,
    "limits": "Static PE verification, not a Windows/game runtime test.",
}
if args.out:
    args.out.write_text(json.dumps(report, indent=2) + "\n")
print(f"PASS: {len(checks)} offline PE fingerprint checks; exact user EXE SHA256 matched.")
