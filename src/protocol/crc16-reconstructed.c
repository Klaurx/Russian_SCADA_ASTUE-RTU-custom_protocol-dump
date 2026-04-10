/*
 * crc16-reconstructed.c
 *
 * Reconstructed CRC algorithms for USOTM field bus frame validation and
 * Modbus frame validation, as used in the firmware.
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
 * Two candidate algorithms are implemented below:
 *
 *   Modbus CRC16 with polynomial 0xa001 (reflected 0x8005): confirmed used
 *     by sirius_mb, sirius_mb_ntu, mdbf, mdbf80, mdbtcp, and stem300/cmdbf
 *     via the auchCRCHi and auchCRCLo static table symbols and the exported
 *     CRC_Update(uint8_t*, uint16_t) symbol.
 *
 *   CRC16 CCITT with polynomial 0x1021 (unreflected): common alternative for
 *     serial bus protocols in embedded systems. Used in many industrial
 *     field bus protocols that coexist with USOTM.
 *
 *
 * Modbus CRC (sirius_mb, mdbf, stem300/cmdbf, mdbtcp)
 *
 * The Modbus CRC is fully confirmed via the auchCRCHi and auchCRCLo symbols
 * (mangled as _ZL9auchCRCHi and _ZL9auchCRCLo in the symbol table) and the
 * exported CRC_Update(uint8_t*, uint16_t) function. The symbols appear in:
 *   sirius_mb, sirius_mb_ntu, mdbf, mdbf80, mdbtcp, and stem300/cmdbf.
 * The polynomial is 0xa001 (reflected 0x8005), initial value 0xffff.
 *
 *
 * Additional CRC variants in the firmware
 *
 * MCRC16(unsigned long, unsigned long) appears in qcet, qpuso (pusclass),
 *   and the Aqalpha binary. It is used in the class-based RTU polling
 *   protocols (ctclass, mirclass, pusclass).
 *
 * RC16(unsigned short, unsigned short) appears in cicpcon (ICP-CON driver).
 *   The ICP-CON protocol uses a different CRC convention.
 *
 * FCRC16(char*, unsigned short) appears in Aqalpha and qpuso. Also FCRC and
 *   FCRCM variants appear in qpuso.
 *
 * CheckCRC(unsigned char*, unsigned short) is a member of cmdbf in the
 *   stem300 and mdbf drivers. It validates Modbus frames using the same
 *   algorithm as CRC_Update.
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
 * the lookup table form and this bit-by-bit form produce identical output
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
 * crc16_ccitt
 *
 * CRC16 CCITT with polynomial 0x1021 (unreflected).
 * Initial value is 0xffff. This is the most common form used in serial
 * bus protocols as an alternative to the Modbus polynomial.
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
 *   This confirms that header (4 bytes) plus CRC (2 bytes) account for
 *   all 6 bytes of overhead beyond the payload.
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
