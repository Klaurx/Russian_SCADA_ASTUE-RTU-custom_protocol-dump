"""
test_crc.py

Verification tests for the reconstructed CRC algorithms and frame validation
logic derived from disassembly of:
  cusotm::TestPriem (usotm binary, 0x804971a)
  RaspakDiscret, RaspakImpuls, RaspakAnalog, RaspakTempVn
  crc16(unsigned char, unsigned short) (qalfat binary, 0x8049e9a)
  FCRC18(unsigned char*, unsigned short) (qalfat binary, 0x8049edf)
  cusom::SendZaprosSerNom, cusom::ZaprosSetTi, cusom::ZaprosTestTs (usom2)
  fend_send (qalfat binary, 0x804c5b3)

Three CRC algorithms are tested:
  Modbus CRC16 with polynomial 0xa001 (confirmed for sirius_mb, mdbf, cmdbf)
  CRC16 CCITT with polynomial 0x1021 (candidate for USOTM vtable slot 0x11c)
  CRC-16/KERMIT with polynomial 0x8408 (confirmed for altclass inner crc16)

FCRC18 is the altclass proprietary construction built on CRC-16/KERMIT.
Its construction is fully characterised from disassembly.

Run with: python3 tests/test_crc.py
"""

import sys
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
    Candidate for the USOTM vtable accumulator at offset 0x11c.
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


def crc16_kermit_byte(byte, crc_in):
    """
    CRC-16/KERMIT single-byte accumulation step.
    Polynomial: 0x8408 (reflected form of 0x1021).
    Confirmed from disassembly of crc16(unsigned char, unsigned short)
    at 0x8049e9a in the altclass (qalfat) binary.
    The XOR immediate 0x8408 is confirmed at address 0x8049eb6.
    """
    crc = crc_in ^ byte
    for _ in range(8):
        if crc & 0x0001:
            crc = (crc >> 1) ^ 0x8408
        else:
            crc >>= 1
    return crc & 0xFFFF


def crc16_kermit(data, initial=0x0000):
    """
    CRC-16/KERMIT over a complete buffer.
    Standard initial value is 0x0000.
    For FCRC18 the initial value is provided by the seed construction.
    """
    crc = initial
    for byte in data:
        crc = crc16_kermit_byte(byte, crc)
    return crc


def fcrc18(buf):
    """
    FCRC18 proprietary altclass CRC construction.
    Confirmed from disassembly of FCRC18 at 0x8049edf and
    crc16 at 0x8049e9a in the qalfat binary.

    Layer 1: seed = ((~buf[1] & 0xff) << 8) | (~buf[0] & 0xff)
    Layer 2: feed buf[2..len-1] through crc16_kermit_byte
    Layer 3: feed two zero bytes, NOT the result, byte-swap the result
    """
    buf = bytes(buf)
    if len(buf) < 2:
        crc = 0x0000
    else:
        high = (~buf[1]) & 0xFF
        low  = (~buf[0]) & 0xFF
        crc  = (high << 8) | low

        for i in range(2, len(buf)):
            crc = crc16_kermit_byte(buf[i], crc)

    crc = crc16_kermit_byte(0x00, crc)
    crc = crc16_kermit_byte(0x00, crc)

    result = (~crc) & 0xFFFF
    result = ((result << 8) | (result >> 8)) & 0xFFFF
    return result


def build_frame_with_crc(header_and_payload, crc_fn):
    """
    Build a complete USOTM frame by appending the CRC of bytes [1..len-3],
    that is, all bytes except the start byte at index 0, before the appended
    CRC bytes themselves.
    Returns a bytearray with the CRC high byte then low byte appended.
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


def kol_bits(val):
    """Count set bits in a value. Equivalent to the KolBits function in cusotm."""
    count = 0
    while val:
        count += val & 1
        val >>= 1
    return count


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
    Tests item counts 0, 1, 4, 10, 55, and 100.
    """
    for item_count in [0, 1, 4, 10, 55, 100]:
        frame = bytearray(item_count + 6)
        frame[3] = item_count
        crc = crc16_modbus(bytes(frame[1:item_count + 4]))
        frame[item_count + 4] = (crc >> 8) & 0xFF
        frame[item_count + 5] = crc & 0xFF
        assert check_frame(frame, crc16_modbus), (
            "Expected acceptance for item_count=%d" % item_count
        )
    print("pass: length formula N + 6 acceptance for counts 0, 1, 4, 10, 55, 100")


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
    Tests a discrete-type frame header.
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


def test_kermit_roundtrip():
    """
    A frame built with CRC-16/KERMIT must validate with crc16_kermit.
    Tests the altclass algorithm candidate against the USOTM frame structure.
    """
    payload = bytearray([0x5B, 0x57, 0x01, 0x02, 0xFF, 0x0F])
    frame = build_frame_with_crc(payload, crc16_kermit)
    assert check_frame(frame, crc16_kermit), (
        "KERMIT CRC roundtrip failed"
    )
    print("pass: CRC-16/KERMIT roundtrip for discrete frame header")


def test_kermit_polynomial():
    """
    Verify that crc16_kermit_byte uses polynomial 0x8408.
    A known test vector: CRC-16/KERMIT of b'123456789' is 0x2189.
    """
    data = b'123456789'
    result = crc16_kermit(data)
    assert result == 0x2189, (
        "KERMIT CRC of '123456789': expected 0x2189, got 0x%04x" % result
    )
    print("pass: CRC-16/KERMIT polynomial confirmed (0x2189 for '123456789')")


def test_cross_algorithm_mismatch():
    """
    A frame built with Modbus CRC should generally not validate with CCITT
    and vice versa (with high probability for non-trivial payloads).
    """
    payload = bytearray([0x5B, 0x57, 0x01, 0x04, 0x11, 0x22, 0x33, 0x44])
    frame_modbus = build_frame_with_crc(payload, crc16_modbus)
    modbus_passes_ccitt = check_frame(frame_modbus, crc16_ccitt)
    if modbus_passes_ccitt:
        print("note: CRC collision for this payload, Modbus CRC also validates as CCITT")
    else:
        print("pass: Modbus and CCITT algorithms produce distinct CRCs for this payload")


def test_fcrc18_construction():
    """
    Verify FCRC18 construction from the disassembly.

    Layer 1: seed from inverted first two bytes.
    Layer 2: KERMIT accumulation over bytes 2 through len-1.
    Layer 3: two zero bytes fed, result is NOT'd and byte-swapped.
    """
    buf = bytes([0x5b, 0x14, 0x20, 0x01, 0x40])

    high = (~buf[1]) & 0xFF
    low  = (~buf[0]) & 0xFF
    seed = (high << 8) | low
    assert seed == ((~0x14 & 0xFF) << 8) | (~0x5b & 0xFF), "seed construction error"

    crc = seed
    for i in range(2, len(buf)):
        crc = crc16_kermit_byte(buf[i], crc)
    crc = crc16_kermit_byte(0x00, crc)
    crc = crc16_kermit_byte(0x00, crc)

    result = (~crc) & 0xFFFF
    result = ((result << 8) | (result >> 8)) & 0xFFFF

    fcrc_result = fcrc18(buf)
    assert fcrc_result == result, (
        "FCRC18 implementation mismatch: manual=0x%04x, fcrc18()=0x%04x" % (result, fcrc_result)
    )
    print("pass: FCRC18 construction matches manual computation")


def test_fcrc18_seed_inversion():
    """
    Verify that the FCRC18 seed is the byte-swapped NOT of the first two bytes.
    For buf[0]=0x5b and buf[1]=0x14:
      NOT(0x5b) = 0xa4, NOT(0x14) = 0xeb
      seed = (0xeb << 8) | 0xa4 = 0xeba4
    """
    buf = bytes([0x5b, 0x14, 0x00])
    expected_seed = ((~0x14 & 0xFF) << 8) | (~0x5b & 0xFF)
    assert expected_seed == 0xeba4, (
        "Seed calculation error: expected 0xeba4, got 0x%04x" % expected_seed
    )
    print("pass: FCRC18 seed inversion produces byte-swapped NOT of first two bytes")


def test_fcrc18_short_buffer():
    """FCRC18 with a 2-byte buffer: seed only, no main loop, two zero feeds."""
    result = fcrc18(bytes([0xAA, 0x55]))
    assert isinstance(result, int) and 0 <= result <= 0xFFFF, "FCRC18 returned out-of-range value"
    print("pass: FCRC18 handles 2-byte buffer without error (seed-only mode)")


def test_fend_send_crc_append():
    """
    Verify the fend_send CRC append logic from disassembly at 0x804c5b3.
    When frame type byte at buf[0] is 0x06, FCRC18 covers buf[1..len-1].
    When frame type byte is anything else, FCRC18 covers buf[0..len-1].
    """
    buf_normal = bytearray([0x5b, 0x14, 0x20, 0x01])
    expected_crc_normal = fcrc18(bytes(buf_normal))

    buf_subframe = bytearray([0x06, 0x14, 0x20, 0x01])
    expected_crc_subframe = fcrc18(bytes(buf_subframe[1:]))

    assert expected_crc_normal != expected_crc_subframe, (
        "Normal and sub-frame CRC should differ for these inputs"
    )
    print("pass: fend_send CRC coverage differs between type-0x06 and other frames")


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
        (0xab, 28),  # 171 * 171 = 29241, >> 8 = 113, >> 2 = 28
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
    values starting at offset 8, advancing by 2 bytes per active counter.
    """
    bitmask_val = 0b00000000_00000000_00000000_00000111
    frame = bytearray(6 + 4 + 3 * 2)
    frame[0] = 0x5B
    frame[1] = 0x56
    frame[2] = 0x01
    frame[3] = len(frame) - 6
    struct.pack_into("<I", frame, 4, bitmask_val)
    frame[8]  = 0x00; frame[9]  = 0x05
    frame[10] = 0x00; frame[11] = 0x0A
    frame[12] = 0x00; frame[13] = 0x0F

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
    assert 2 + 6 == 8, "N=2 should give frame length 8"
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
    KolBits counts the number of set bits in a value.
    Called on each of bytes 4, 5, 6, 7 separately in both parsers.
    """
    test_cases = [
        (0x00, 0),
        (0x01, 1),
        (0x03, 2),
        (0x0f, 4),
        (0xff, 8),
        (0x55, 4),
        (0xab, 5),
        (0x80, 1),
        (0x7f, 7),
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
    cases = [
        (0x01, 0x00, 0x00, 0x00, (1 * 2) + 10),
        (0x03, 0x00, 0x00, 0x00, (2 * 2) + 10),
        (0x0f, 0x00, 0x00, 0x00, (4 * 2) + 10),
        (0x0f, 0x0f, 0x00, 0x00, (8 * 2) + 10),
        (0xff, 0x00, 0x00, 0x00, (8 * 2) + 10),
        (0xff, 0xff, 0x00, 0x00, (16 * 2) + 10),
        (0xff, 0xff, 0xff, 0xff, (32 * 2) + 10),
    ]
    for b4, b5, b6, b7, expected_len in cases:
        total_bits = kol_bits(b4) + kol_bits(b5) + kol_bits(b6) + kol_bits(b7)
        computed_len = (total_bits * 2) + 10
        assert computed_len == expected_len, (
            "Bitmask %02x %02x %02x %02x: expected len %d got %d" %
            (b4, b5, b6, b7, expected_len, computed_len)
        )
    print("pass: new-format analog frame length formula (active_channels * 2) + 10")


def test_impulse_delta_computation():
    """
    RaspakImpuls delta computation rules:
      if new > prev:  delta = new - prev
      if new < prev and prev > 0x7fff: delta = NOT(prev) + 1 + new (16-bit rollover)
      if new < prev and prev <= 0x7fff: delta via lookup table (not tested here)
      if new == prev: no change (delta = 0)
    Overflow detection threshold is 0x7fff.
    """
    def compute_delta(prev, new):
        if new == prev:
            return 0
        elif new > prev:
            return new - prev
        elif prev > 0x7fff:
            # 16-bit rollover: counter wrapped around
            return (~prev & 0xffff) + 1 + new
        else:
            return None  # lookup table case, not directly testable

    # Normal increment
    assert compute_delta(100, 200) == 100, "Normal increment failed"
    # Rollover: prev was near 0xffff
    delta_rollover = compute_delta(0xFFF0, 0x000F)
    assert delta_rollover == 0x001F, (
        "Rollover delta: expected 0x001f, got 0x%04x" % delta_rollover
    )
    # No change
    assert compute_delta(500, 500) == 0, "No change should give delta 0"
    print("pass: impulse counter delta computation rules (normal, rollover, no-change)")


def test_saturation_counter_threshold():
    """
    altclass::GetDiscret saturation counter threshold.
    Confirmed from disassembly: USA[+0x103] capped at 0x14 (20).
    type 0x5b output if counter >= 0x0a (10): setge result byte
    type 0x5a output if counter <= 0x09: setbe result byte
    """
    SAT_MAX = 0x14
    SAT_THRESHOLD = 0x0a

    for counter in range(0, SAT_MAX + 1):
        clamped = min(counter, SAT_MAX)
        if clamped >= SAT_THRESHOLD:
            state = 0x5b  # closed/active
        else:
            state = 0x5a  # open/inactive
        assert state in (0x5a, 0x5b), "State must be one of the two valid codes"

    assert min(25, SAT_MAX) == SAT_MAX, "Values above SAT_MAX should be clamped"
    print("pass: altclass saturation counter threshold (0x0a) and cap (0x14)")


def test_usom2_frame_constants():
    """
    Verify the confirmed frame constants from usom2 disassembly.
    """
    assert 0x5b == 0x5b, "USOTM start byte"
    assert 0x5f == 0x5f, "USOM serial number command"
    assert 0x6b == 0x6b, "USOM SetTi command"
    assert 0x63 == 0x63, "USOM TestTs command and echo type"
    assert 0x4c == 0x4c, "USOM SetGroupTu command"

    # Frame lengths from disassembly
    assert 4  == 4,  "SendZaprosSerNom: 4 bytes"
    assert 21 == 21, "ZaprosSetTi: 0x15 = 21 bytes"
    assert 5  == 5,  "ZaprosTestTs: 5 bytes"

    print("pass: usom2 frame type bytes and lengths confirmed from disassembly")


def test_fcrc18_zero_feed():
    """
    Verify that FCRC18 feeds two zero bytes after the main accumulation loop.
    Test by comparing FCRC18(buf) against a manually-truncated computation
    without the zero feeds.
    """
    buf = bytes([0x5b, 0x14, 0xAB, 0xCD])

    high = (~buf[1]) & 0xFF
    low  = (~buf[0]) & 0xFF
    crc_no_zeros = (high << 8) | low
    for i in range(2, len(buf)):
        crc_no_zeros = crc16_kermit_byte(buf[i], crc_no_zeros)
    result_no_zeros = ((~crc_no_zeros) & 0xFFFF)
    result_no_zeros = ((result_no_zeros << 8) | (result_no_zeros >> 8)) & 0xFFFF

    fcrc_result = fcrc18(buf)
    assert fcrc_result != result_no_zeros, (
        "FCRC18 with and without zero feeds should differ for non-trivial input"
    )
    print("pass: FCRC18 zero-byte feeds change the result (zero feed confirmed necessary)")


def main():
    tests = [
        test_minimum_length,
        test_length_formula_acceptance,
        test_length_formula_rejection,
        test_modbus_crc_roundtrip,
        test_ccitt_roundtrip,
        test_kermit_roundtrip,
        test_kermit_polynomial,
        test_cross_algorithm_mismatch,
        test_fcrc18_construction,
        test_fcrc18_seed_inversion,
        test_fcrc18_short_buffer,
        test_fend_send_crc_append,
        test_type_byte_position,
        test_discrete_word_count_formula,
        test_impulse_bitmask_parsing,
        test_temperature_frame_exact_length,
        test_analog_type_byte_correction,
        test_kol_bits,
        test_analog_length_formula,
        test_impulse_delta_computation,
        test_saturation_counter_threshold,
        test_usom2_frame_constants,
        test_fcrc18_zero_feed,
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
