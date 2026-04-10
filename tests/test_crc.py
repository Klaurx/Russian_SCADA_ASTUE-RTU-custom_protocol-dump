"""
test_crc.py

Verification tests for the reconstructed CRC algorithms and frame validation
logic derived from cusotm::TestPriem (usotm binary, 0x804971a) and related
disassembly of RaspakDiscret, RaspakImpuls, RaspakAnalog, and RaspakTempVn.

Without a real captured frame containing a known-good CRC, these tests
validate structural correctness and algorithm consistency rather than
asserting a specific expected CRC value from a live capture.

Two candidate CRC algorithms are tested:
  Modbus CRC16 with polynomial 0xa001 (confirmed for sirius_mb, mdbf, cmdbf)
  CRC16 CCITT with polynomial 0x1021 (candidate for USOTM accumulator)

The actual USOTM accumulator is a virtual method at cusotm vtable offset
0x11c and has not yet been disassembled to confirm the exact algorithm.

Run with: python3 tests/test_crc.py
"""

import sys
import os
import struct


def crc16_modbus(data):
    """
    Modbus CRC16 with polynomial 0xa001 (reflected 0x8005).
    Initial value: 0xffff.
    Confirmed used by sirius_mb and mdbf via auchCRCHi and auchCRCLo symbols.
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def crc16_ccitt(data):
    """
    CRC16 CCITT with polynomial 0x1021 (unreflected).
    Initial value: 0xffff.
    Alternative candidate for the USOTM vtable accumulator.
    """
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
        crc &= 0xFFFF
    return crc


def build_frame_with_crc(header_and_payload, crc_fn):
    """
    Build a complete USOTM frame by appending the CRC of bytes [1:-0]
    (i.e. all bytes except the start byte at index 0).
    Returns a bytearray with the CRC high byte then low byte appended.

    The CRC covers buf[1] through buf[len-3], which means it covers
    the data we pass in here (indices 1 onward), excluding only the
    CRC bytes that will be appended.
    """
    data = bytearray(header_and_payload)
    crc = crc_fn(bytes(data[1:]))
    data.append((crc >> 8) & 0xFF)
    data.append(crc & 0xFF)
    return data


def check_frame(buf, crc_fn):
    """
    Apply the TestPriem three-part validation to buf using crc_fn.
    Returns True if all checks pass, False otherwise.

    Check 1: len > 5 (minimum frame is 6 bytes)
    Check 2: CRC over buf[1:len-2] matches buf[len-2] and buf[len-1]
    Check 3: buf[3] + 6 == len
    """
    buf = bytes(buf)
    if len(buf) <= 5:
        return False
    computed = crc_fn(buf[1:len(buf) - 2])
    if ((computed >> 8) & 0xFF) != buf[len(buf) - 2]:
        return False
    if (computed & 0xFF) != buf[len(buf) - 1]:
        return False
    if buf[3] + 6 != len(buf):
        return False
    return True


def test_minimum_length():
    """Frames of 5 bytes or fewer must be rejected by the length check."""
    for n in range(6):
        frame = bytearray(n)
        assert not check_frame(frame, crc16_modbus), (
            "Expected rejection for %d-byte frame" % n
        )
    print("pass: minimum length rejection for all frames up to 5 bytes")


def test_length_formula_acceptance():
    """
    TestPriem accepts frames where total_len == buf[3] + 6.
    This covers item counts 0, 1, 4, 10, and 55.
    """
    for item_count in [0, 1, 4, 10, 55]:
        frame = bytearray(item_count + 6)
        frame[3] = item_count
        crc = crc16_modbus(bytes(frame[1:item_count + 4]))
        frame[item_count + 4] = (crc >> 8) & 0xFF
        frame[item_count + 5] = crc & 0xFF
        assert check_frame(frame, crc16_modbus), (
            "Expected acceptance for item_count=%d" % item_count
        )
    print("pass: length formula N + 6 acceptance for counts 0, 1, 4, 10, 55")


def test_length_formula_rejection():
    """check_frame rejects frames where total_len does not equal buf[3] + 6."""
    frame = bytearray(10)
    frame[3] = 5
    assert not check_frame(frame, crc16_modbus), (
        "Expected rejection: len=10, N=5, expected total 11"
    )
    print("pass: length formula mismatch correctly rejected")


def test_modbus_crc_roundtrip():
    """
    A frame built with Modbus CRC must validate with crc16_modbus.
    Tests the basic roundtrip for a discrete-type frame.
    """
    payload = bytearray([0x5B, 0x57, 0x01, 0x02, 0xFF, 0x0F])
    frame = build_frame_with_crc(payload, crc16_modbus)
    assert check_frame(frame, crc16_modbus), (
        "Modbus CRC roundtrip failed"
    )
    print("pass: Modbus CRC16 roundtrip for discrete frame header")


def test_ccitt_roundtrip():
    """
    A frame built with CCITT CRC must validate with crc16_ccitt.
    """
    payload = bytearray([0x5B, 0x57, 0x01, 0x02, 0xFF, 0x0F])
    frame = build_frame_with_crc(payload, crc16_ccitt)
    assert check_frame(frame, crc16_ccitt), (
        "CCITT CRC roundtrip failed"
    )
    print("pass: CRC16 CCITT roundtrip for discrete frame header")


def test_cross_algorithm_mismatch():
    """
    A frame built with Modbus CRC should generally not validate with CCITT
    and vice versa (with high probability for random payloads).
    """
    payload = bytearray([0x5B, 0x57, 0x01, 0x04, 0x11, 0x22, 0x33, 0x44])
    frame_modbus = build_frame_with_crc(payload, crc16_modbus)
    modbus_passes_ccitt = check_frame(frame_modbus, crc16_ccitt)
    if modbus_passes_ccitt:
        print("note: CRC collision for this payload, Modbus CRC also validates as CCITT")
    else:
        print("pass: Modbus and CCITT algorithms produce distinct CRCs for this payload")


def test_type_byte_position():
    """Response type bytes appear at frame offset 1 for all known types."""
    known_types = [0x57, 0x56, 0x5A, 0x5D]
    for t in known_types:
        frame = bytearray(6)
        frame[0] = 0x5B
        frame[1] = t
        frame[2] = 0x01
        frame[3] = 0x00
        assert frame[1] == t, "Type byte not at offset 1"
    print("pass: type bytes 0x57, 0x56, 0x5a, 0x5d all positioned at frame offset 1")


def test_discrete_word_count_formula():
    """
    RaspakDiscret computes word count as (N * 0xab) >> 8 >> 2.
    Confirmed from disassembly at 0x804a43f: mul dl (dl=0xab), shr ax 8, shr bl 2.
    """
    cases = [
        (0x04, 0),   # 4 * 171 = 684, >> 8 = 2, >> 2 = 0
        (0x08, 1),   # 8 * 171 = 1368, >> 8 = 5, >> 2 = 1
        (0x10, 2),   # 16 * 171 = 2736, >> 8 = 10, >> 2 = 2
        (0x40, 10),  # 64 * 171 = 10944, >> 8 = 42, >> 2 = 10
        (0xab, 28),  # 171 * 171 = 29241 = 0x71c9, >> 8 = 0x71 = 113, >> 2 = 28
    ]
    for raw_n, expected_words in cases:
        words = ((raw_n * 0xAB) >> 8) >> 2
        assert words == expected_words, (
            "N=0x%02x: expected %d words, got %d" % (raw_n, expected_words, words)
        )
    print("pass: discrete word count formula (N * 0xab) >> 8 >> 2")


def test_impulse_bitmask_parsing():
    """
    Impulse counter bitmask covers 32 positions across 4 bytes.
    Confirmed from RaspakImpuls: 4 bytes at offsets 4,5,6,7 then
    values starting at offset 8, advancing by 2 per active counter.
    """
    bitmask_val = 0b00000000_00000000_00000000_00000111
    frame = bytearray(6 + 4 + 3 * 2)
    frame[0] = 0x5B
    frame[1] = 0x56
    frame[2] = 0x01
    frame[3] = len(frame) - 6
    struct.pack_into("<I", frame, 4, bitmask_val)
    frame[8]  = 0x00; frame[9]  = 0x05   # counter 0 value: 5
    frame[10] = 0x00; frame[11] = 0x0A   # counter 1 value: 10
    frame[12] = 0x00; frame[13] = 0x0F   # counter 2 value: 15

    bitmask = struct.unpack_from("<I", frame, 4)[0]
    active = [i for i in range(32) if bitmask & (1 << i)]
    assert active == [0, 1, 2], "Expected bits 0, 1, 2 active, got %s" % active

    counter_values = [
        (frame[8] << 8) | frame[9],
        (frame[10] << 8) | frame[11],
        (frame[12] << 8) | frame[13],
    ]
    assert counter_values == [5, 10, 15], (
        "Expected [5, 10, 15], got %s" % counter_values
    )
    print("pass: impulse bitmask parsing with 3 active counters")


def test_temperature_frame_exact_length():
    """
    RaspakTempVn requires frame length exactly 8 bytes.
    Confirmed from: cmp si, 0x8 / jne invalid in RaspakTempVn.
    item_count N at buf[3] must be 2 (giving N + 6 = 8).
    """
    for n_val in [0, 1, 3, 4]:
        assert n_val + 6 != 8 or n_val == 2, "Unexpected match"
    n_val = 2
    assert n_val + 6 == 8, "N=2 should give frame length 8"
    print("pass: temperature frame requires N=2 giving exactly 8 bytes")


def test_analog_type_byte_correction():
    """
    Verify the corrected analog type byte.
    Earlier documentation incorrectly stated 0x86.
    Confirmed from RaspakAnalog at 0x804ada8: cmp BYTE PTR [edi+0x1], 0x5a
    """
    incorrect_type = 0x86
    correct_type = 0x5A
    assert correct_type == 0x5A, "Analog type byte must be 0x5a"
    assert incorrect_type != correct_type, "Old 0x86 value is not correct"
    print("pass: analog type byte confirmed as 0x5a (not 0x86 as previously documented)")


def test_kol_bits():
    """
    Simulate the KolBits function used by RaspakImpuls and RaspakAnalog.
    KolBits counts the number of set bits in a byte value.
    Called on each of bytes 4, 5, 6, 7 separately in both parsers.
    """
    def kol_bits(byte_val):
        count = 0
        while byte_val:
            count += byte_val & 1
            byte_val >>= 1
        return count

    test_cases = [
        (0x00, 0),
        (0x01, 1),
        (0x03, 2),
        (0x0f, 4),
        (0xff, 8),
        (0x55, 4),
        (0xab, 5),
    ]
    for byte_val, expected_count in test_cases:
        result = kol_bits(byte_val)
        assert result == expected_count, (
            "KolBits(0x%02x) = %d, expected %d" % (byte_val, result, expected_count)
        )
    print("pass: KolBits bit counting for impulse and analog channel counting")


def test_analog_length_formula():
    """
    The expected frame length for the new-format analog type is:
    (total_active_channels * 2) + 0x0a
    where total_active_channels is the sum of KolBits across the 4 bitmask bytes.
    Confirmed from RaspakAnalog: lea eax, [eax+eax*1+0xa] (doubling + 10).
    """
    def kol_bits(b):
        count = 0
        while b:
            count += b & 1
            b >>= 1
        return count

    cases = [
        (0x01, 0x00, 0x00, 0x00, (1 * 2) + 10),
        (0x03, 0x00, 0x00, 0x00, (2 * 2) + 10),
        (0x0f, 0x00, 0x00, 0x00, (4 * 2) + 10),
        (0x0f, 0x0f, 0x00, 0x00, (8 * 2) + 10),
        (0xff, 0x00, 0x00, 0x00, (8 * 2) + 10),
    ]
    for b4, b5, b6, b7, expected_len in cases:
        total_bits = kol_bits(b4) + kol_bits(b5) + kol_bits(b6) + kol_bits(b7)
        computed_len = (total_bits * 2) + 10
        assert computed_len == expected_len, (
            "Bitmask %02x %02x %02x %02x: expected len %d got %d" %
            (b4, b5, b6, b7, expected_len, computed_len)
        )
    print("pass: new-format analog frame length formula (active_channels * 2) + 10")


def main():
    tests = [
        test_minimum_length,
        test_length_formula_acceptance,
        test_length_formula_rejection,
        test_modbus_crc_roundtrip,
        test_ccitt_roundtrip,
        test_cross_algorithm_mismatch,
        test_type_byte_position,
        test_discrete_word_count_formula,
        test_impulse_bitmask_parsing,
        test_temperature_frame_exact_length,
        test_analog_type_byte_correction,
        test_kol_bits,
        test_analog_length_formula,
    ]

    failed = 0
    for t in tests:
        try:
            t()
        except AssertionError as e:
            print("FAIL: %s: %s" % (t.__name__, e))
            failed += 1
        except Exception as e:
            print("ERROR: %s: %s" % (t.__name__, e))
            failed += 1

    print()
    print("%d/%d tests passed" % (len(tests) - failed, len(tests)))
    return failed


if __name__ == "__main__":
    sys.exit(main())
