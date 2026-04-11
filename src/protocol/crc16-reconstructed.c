/*
 * crc16-reconstructed.c
 *
 * Reconstructed CRC algorithms for USOTM field bus frame validation,
 * Modbus frame validation, and the altclass FCRC18 construction.
 *
 *
 * USOTM CRC (cusotm::TestPriem at 0x804971a)
 *
 * The CRC covers bytes 1 through len-3 inclusive. This means the start byte
 * at offset 0 is excluded, and the two CRC bytes at the frame tail are also
 * excluded. The accumulation is performed by a virtual method at vtable offset
 * 0x11c in the cusotm vtable. The result is split into high and low bytes and
 * compared against buf[len-2] and buf[len-1].
 *
 * The exact polynomial is not directly visible from disassembly of TestPriem
 * because the accumulation call goes through a vtable slot. Disassembly of
 * that vtable slot is required to confirm which algorithm is used.
 *
 * Two candidate algorithms for the vtable slot:
 *
 *   Modbus CRC16 with polynomial 0xa001 (reflected 0x8005): confirmed used
 *     by sirius_mb, sirius_mb_ntu, mdbf, mdbf80, mdbtcp, and stem300/cmdbf
 *     via the auchCRCHi and auchCRCLo static table symbols and the exported
 *     CRC_Update(uint8_t*, uint16_t) symbol.
 *
 *   CRC16 CCITT with polynomial 0x1021 (unreflected): common alternative for
 *     serial bus protocols in embedded systems.
 *
 *
 * altclass CRC (inner crc16 at 0x8049e9a)
 *
 * The crc16(unsigned char, unsigned short) function in qalfat is confirmed
 * from disassembly as CRC-16/KERMIT (polynomial 0x8408, the reflected form
 * of 0x1021). The XOR immediate 0x8408 is confirmed at address 0x8049eb6
 * in the bit loop.
 *
 * FCRC18 at 0x8049edf uses crc16_kermit as its inner accumulator but
 * applies a non-standard three-layer construction:
 *   Layer 1: seed = ((~buf[1] & 0xff) << 8) | (~buf[0] & 0xff)
 *   Layer 2: feed buf[2..len-1] through crc16_kermit_byte one byte at a time
 *   Layer 3: feed two zero bytes, NOT the result, byte-swap the result
 *
 *
 * Modbus CRC (sirius_mb, mdbf, stem300/cmdbf, mdbtcp)
 *
 * The Modbus CRC is fully confirmed via the auchCRCHi and auchCRCLo symbols
 * (mangled as _ZL9auchCRCHi and _ZL9auchCRCLo in the symbol table) and the
 * exported CRC_Update(uint8_t*, uint16_t) function. The polynomial is 0xa001
 * (reflected 0x8005), initial value 0xffff.
 *
 *
 * Additional CRC variants in the firmware (not implemented here)
 *
 * MCRC16(unsigned long, unsigned long) appears in qcet, qpuso (pusclass),
 *   and the Aqalpha binary. It is used in the class-based RTU polling
 *   protocols (ctclass, mirclass, pusclass). The two-argument form suggests
 *   it operates on pre-assembled 32-bit data words rather than byte buffers.
 *
 * RC16(unsigned short, unsigned short) appears in cicpcon, qptym, and qpuso.
 *   Two 16-bit arguments, likely a word-at-a-time accumulation variant.
 *
 * FCRC16(char*, unsigned short) appears in Aqalpha, qmir, and qptym.
 *   Standard buffer CRC with the FCRC naming convention. Likely CRC-16/KERMIT
 *   without the FCRC18 seed inversion and byte-swap.
 *
 * FCRC(unsigned char*, unsigned short) appears in qcet and qpty.
 *   Base variant of the FCRC family.
 *
 * FCRCM(unsigned char*, unsigned int) appears in qpuso.
 *   Variant with unsigned int length parameter.
 *
 * CheckCRC(unsigned char*, unsigned short) is a member of cmdbf in the
 *   stem300, mdbf80, and mdbfo drivers. It validates Modbus frames using
 *   the same algorithm as CRC_Update.
 *
 * MakeCrc(unsigned char*, int) appears in libjevent.so.1 for journal
 *   record integrity checking.
 *
 *
 * Authorization: view-only research license. See LICENSE.
 */

#include "crc16.h"
#include <stdint.h>
#include <stddef.h>


/*
 * crc16_modbus
 *
 * Standard Modbus CRC16 with polynomial 0xa001 (reflected 0x8005).
 * Initial value is 0xffff. This is the bit-by-bit implementation.
 *
 * The firmware uses a lookup-table form (auchCRCHi and auchCRCLo), but
 * the lookup-table form and this bit-by-bit form produce identical output
 * for all inputs.
 */
uint16_t
crc16_modbus(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}


/*
 * crc16_kermit_byte
 *
 * CRC-16/KERMIT single-byte accumulation step.
 * Polynomial: 0x8408 (reflected form of 0x1021).
 *
 * Confirmed from disassembly of crc16(unsigned char, unsigned short)
 * at address 0x8049e9a in the altclass (qalfat) binary:
 *
 *   8049ea5:  b9 08 00 00 00  mov ecx, 0x8     ; 8 bit iterations
 *   8049eaa:  89 d0           mov eax, edx     ; crc working copy
 *   [loop body]:
 *     f6 c0 01    test al, 0x1        ; check LSB
 *     74 06       je (skip xor)
 *     d1 e8       shr eax, 1         ; shift right
 *     35 08 84 00 00  xor eax, 0x8408  ; XOR with polynomial
 *     [skip xor]:
 *     d1 e8       shr eax, 1         ; shift right (no XOR case)
 *   e2 xx  loop (8 times)
 *
 * The confirmed instruction at 0x8049eb6 is the XOR with 0x8408.
 *
 * Input:
 *   byte:   the byte to accumulate
 *   crc_in: the running CRC value (seed for the first byte)
 * Returns:
 *   updated CRC value after processing the input byte
 */
uint16_t
crc16_kermit_byte(uint8_t byte, uint16_t crc_in)
{
    uint16_t crc = crc_in ^ (uint16_t)byte;
    int j;

    for (j = 0; j < 8; j++) {
        if (crc & 0x0001) {
            crc = (crc >> 1) ^ 0x8408;
        } else {
            crc >>= 1;
        }
    }
    return crc;
}


/*
 * crc16_kermit
 *
 * CRC-16/KERMIT over a complete buffer.
 * Initial value: 0x0000 (standard KERMIT initialisation).
 *
 * Calls crc16_kermit_byte for each byte in the buffer.
 */
uint16_t
crc16_kermit(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0x0000;
    size_t i;

    for (i = 0; i < len; i++) {
        crc = crc16_kermit_byte(buf[i], crc);
    }
    return crc;
}


/*
 * crc16_ccitt
 *
 * CRC16 CCITT with polynomial 0x1021 (unreflected).
 * Initial value is 0xffff. This is the most common form used in serial
 * bus protocols as an alternative to the Modbus polynomial.
 *
 * Candidate for the cusotm vtable accumulator at offset 0x11c.
 * Pending confirmation via disassembly of the vtable slot.
 */
uint16_t
crc16_ccitt(const uint8_t *buf, size_t len)
{
    uint16_t crc = 0xFFFF;
    size_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= ((uint16_t)buf[i] << 8);
        for (j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}


/*
 * fcrc18
 *
 * The altclass FCRC18 proprietary CRC construction.
 * Confirmed from disassembly of FCRC18 at 0x8049edf and crc16 at 0x8049e9a.
 *
 * Construction:
 *   Layer 1: seed = ((~buf[1] & 0xff) << 8) | (~buf[0] & 0xff)
 *   Layer 2: feed buf[2..len-1] through crc16_kermit_byte
 *   Layer 3: feed two zero bytes, NOT the result, byte-swap the result
 *
 * The double zero feed at the end is a deliberate protocol design choice
 * ensuring the CRC covers a defined minimum number of bytes.
 *
 * The final NOT and byte-swap ensure the output cannot be mistaken for a
 * standard KERMIT CRC value.
 *
 * Note on buffer coverage:
 *   When called from fend_send with a full frame: FCRC18(buf, KolSend)
 *     covers buf[0..KolSend-1], with buf[0] and buf[1] used as the seed
 *     and buf[2..KolSend-1] accumulated in the main loop.
 *   When called from fend_send with type byte 0x6 sub-frame:
 *     FCRC18(buf+1, KolSend-1) shifts the coverage by one byte.
 *
 * This is distinct from the USOTM TestPriem CRC coverage which explicitly
 * excludes buf[0] (the start byte). FCRC18 includes buf[0] in the seed.
 */
uint16_t
fcrc18(const uint8_t *buf, uint16_t len)
{
    uint16_t crc;
    uint16_t result;
    uint16_t i;

    if (len < 2) {
        /* degenerate case: not enough bytes to form a seed */
        crc = 0x0000;
    } else {
        /* Layer 1: construct the seed from the inverted first two bytes.
         * ~buf[1] goes into the high byte, ~buf[0] goes into the low byte.
         * This is a byte-swap of the inverted pair, encoding both bytes
         * in a non-obvious order that requires knowledge of the algorithm
         * to reverse. */
        crc = (uint16_t)(((~buf[1] & 0xFFu) << 8) | (~buf[0] & 0xFFu));

        /* Layer 2: accumulate bytes 2 through len-1 using CRC-16/KERMIT. */
        for (i = 2; i < len; i++) {
            crc = crc16_kermit_byte(buf[i], crc);
        }
    }

    /* Layer 3a: feed two zero bytes through the accumulator.
     * This extends the CRC coverage by two virtual zero bytes, ensuring
     * even very short frames contribute at least two rounds of CRC mixing. */
    crc = crc16_kermit_byte(0x00, crc);
    crc = crc16_kermit_byte(0x00, crc);

    /* Layer 3b: bitwise NOT the result. */
    result = ~crc & 0xFFFFu;

    /* Layer 3c: byte-swap the result.
     * ((result << 8) | (result >> 8)) & 0xffff */
    result = (uint16_t)(((result << 8) & 0xFF00u) | ((result >> 8) & 0x00FFu));

    return result;
}


/*
 * usotm_frame_check
 *
 * Applies the three-part TestPriem validation logic to a raw buffer.
 * The crc_fn argument selects which candidate CRC algorithm to use.
 *
 * Check 1: minimum length. The frame must be at least 6 bytes long.
 *   From the disassembly: cmp di, 0x5 / jbe invalid.
 *   This rejects frames of length 5 or fewer.
 *
 * Check 2: CRC verification. The CRC covers buf[1] through buf[len-3].
 *   The start byte at buf[0] is excluded. The CRC bytes at buf[len-2]
 *   and buf[len-1] are also excluded. The computed CRC is split:
 *   high byte = (crc >> 8) & 0xff, compared to buf[len-2]
 *   low byte  = crc & 0xff,         compared to buf[len-1]
 *
 * Check 3: length consistency. buf[3] holds the payload item count N.
 *   The equation N + 6 must equal the actual frame length.
 *
 * Returns 1 if all three checks pass, 0 if any check fails.
 */
int
usotm_frame_check(const uint8_t *buf, uint16_t len,
                  uint16_t (*crc_fn)(const uint8_t *, size_t))
{
    uint16_t computed;
    uint8_t payload_count;

    /* Check 1: minimum length */
    if (len <= 5)
        return 0;

    /* Check 2: CRC over buf[1] through buf[len-3] */
    computed = crc_fn(buf + 1, (size_t)(len - 3));

    if (((computed >> 8) & 0xFF) != buf[len - 2])
        return 0;

    if ((computed & 0xFF) != buf[len - 1])
        return 0;

    /* Check 3: length consistency */
    payload_count = buf[3];
    if ((uint16_t)(payload_count + 6) != len)
        return 0;

    return 1;
}


/*
 * usotm_frame_check_kermit
 *
 * Variant of usotm_frame_check using the CRC-16/KERMIT accumulator.
 * Convenience wrapper for testing the altclass CRC in a USOTM frame context.
 *
 * Note: this uses the standard KERMIT initial value (0x0000) applied to
 * the range buf[1..len-3], which is different from how FCRC18 uses the
 * KERMIT polynomial. FCRC18 constructs a non-zero seed and applies it
 * differently. This function tests the hypothesis that the USOTM vtable
 * slot uses plain KERMIT without the FCRC18 layering.
 */
int
usotm_frame_check_kermit(const uint8_t *buf, uint16_t len)
{
    uint16_t computed;
    uint8_t payload_count;

    if (len <= 5)
        return 0;

    computed = crc16_kermit(buf + 1, (size_t)(len - 3));

    if (((computed >> 8) & 0xFF) != buf[len - 2])
        return 0;

    if ((computed & 0xFF) != buf[len - 1])
        return 0;

    payload_count = buf[3];
    if ((uint16_t)(payload_count + 6) != len)
        return 0;

    return 1;
}
