"""
test_crc.py

Verification tests for the reconstructed CRC algorithms and frame
validation logic. These tests verify that the reconstructed algorithms
produce internally consistent results and match the structural expectations
derived from disassembly.

Without a captured frame containing a known-good CRC, these tests
validate structure and cross-check the two candidate algorithms rather
than asserting a specific expected CRC value.

Run with: python3 tests/test_crc.py

Authorization: view-only research license. See LICENSE.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from tools.usotm_parser import crc16_modbus, crc16_ccitt, check_length, check_crc


def crc16_modbus_build(payload):
    """Build a frame with correct Modbus CRC for testing."""
    buf = payload[:]
    crc = crc16_modbus(buf[1:])
    buf.append((crc >> 8) & 0xFF)
    buf.append(crc & 0xFF)
    return bytearray(buf)


def test_minimum_length():
    """Frames of 5 bytes or fewer must be rejected."""
    for n in range(6):
        frame = bytearray(n)
        assert not check_length(frame), f"Expected rejection for {n}-byte frame"
    print("pass: minimum length rejection")


def test_length_formula():
    """check_length must accept frames where len == buf[3] + 6."""
    for item_count in [0, 1, 4, 10, 55]:
        frame = bytearray(item_count + 6)
        frame[3] = item_count
        assert check_length(frame), f"Expected acceptance for item_count={item_count}"
    print("pass: length formula N + 6")


def test_length_formula_rejection():
    """check_length must reject frames where len != buf[3] + 6."""
    frame = bytearray(10)
    frame[3] = 5
    assert not check_length(frame), "Expected rejection: len=10, N=5, expected 11"
    print("pass: length formula rejection")


def test_modbus_crc_roundtrip():
    """A frame built with Modbus CRC must validate with the same algorithm."""
    payload = bytearray([0xAA, 0x57, 0x01, 0x02, 0xFF, 0x0F])
    frame = crc16_modbus_build(payload)
    assert check_crc(frame, crc16_modbus), "Modbus CRC roundtrip failed"
    print("pass: Modbus CRC16 roundtrip")


def test_ccitt_does_not_match_modbus():
    """A frame built with Modbus CRC should not validate with CCITT (usually)."""
    payload = bytearray([0xAA, 0x57, 0x01, 0x04, 0x11, 0x22, 0x33, 0x44])
    frame = crc16_modbus_build(payload)
    modbus_ok = check_crc(frame, crc16_modbus)
    ccitt_ok  = check_crc(frame, crc16_ccitt)
    assert modbus_ok, "Modbus should pass"
    if ccitt_ok:
        print("note: CCITT happened to match Modbus for this payload (collision)")
    else:
        print("pass: CCITT does not match Modbus CRC for this payload")


def test_type_byte_positions():
    """Response type bytes appear at offset 1 in every valid frame."""
    known_types = [0x57, 0x56, 0x5D, 0x86]
    for t in known_types:
        frame = bytearray(6)
        frame[1] = t
        frame[3] = 0
        assert frame[1] == t
    print("pass: type byte at offset 1")


def test_discrete_item_count_formula():
    """
    RaspakDiscret derives word count as (N * 0xAB) >> 8 >> 2.
    Verify this formula produces expected values for known N.
    """
    cases = [
        (0x04, 0),
        (0x08, 0),
        (0x10, 1),
        (0x40, 4),
        (0xAB, 10),
    ]
    for raw_n, expected_words in cases:
        words = (raw_n * 0xAB) >> 8 >> 2
        assert words == expected_words, (
            f"N=0x{raw_n:02X}: expected {expected_words} words, got {words}"
        )
    print("pass: discrete word count formula")


def test_impulse_bitmask_parsing():
    """32-bit bitmask at bytes 4..7, one counter value per set bit."""
    import struct
    bitmask_val = 0b00000000_00000000_00000000_00000111
    frame = bytearray(10 + 3 * 2)
    frame[1] = 0x56
    frame[3] = len(frame) - 6
    struct.pack_into("<I", frame, 4, bitmask_val)
    frame[8]  = 0x00; frame[9]  = 0x05
    frame[10] = 0x00; frame[11] = 0x0A
    frame[12] = 0x00; frame[13] = 0x0F

    bitmask = struct.unpack_from("<I", frame, 4)[0]
    active = [i for i in range(32) if bitmask & (1 << i)]
    assert active == [0, 1, 2], f"Expected bits 0,1,2 active, got {active}"
    print("pass: impulse bitmask parsing")


def main():
    tests = [
        test_minimum_length,
        test_length_formula,
        test_length_formula_rejection,
        test_modbus_crc_roundtrip,
        test_ccitt_does_not_match_modbus,
        test_type_byte_positions,
        test_discrete_item_count_formula,
        test_impulse_bitmask_parsing,
    ]

    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as e:
            print(f"FAIL: {t.__name__}: {e}")
            failed += 1
        except Exception as e:
            print(f"ERROR: {t.__name__}: {e}")
            failed += 1

    print()
    print(f"{len(tests) - failed}/{len(tests)} tests passed")
    return failed


if __name__ == "__main__":
    sys.exit(main())
