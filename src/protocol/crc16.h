/*
 * crc16.h
 *
 * CRC algorithm interface for USOTM, USOM, Modbus, and altclass frame
 * validation.
 *
 * Four distinct CRC algorithms are present across the firmware:
 *
 *   Modbus CRC16 (polynomial 0xa001, reflected 0x8005):
 *     Confirmed used by sirius_mb, sirius_mb_ntu, mdbf, mdbtcp, and
 *     stem300/cmdbf via the auchCRCHi and auchCRCLo symbol names and the
 *     exported CRC_Update(uint8_t*, uint16_t) function.
 *
 *   CRC-16/KERMIT (polynomial 0x8408, reflected 0x1021):
 *     Confirmed used as the inner crc16(unsigned char, unsigned short)
 *     function in the altclass (qalfat) binary. This is the accumulator
 *     inside FCRC18. The polynomial 0x8408 is confirmed from disassembly
 *     of crc16 at 0x8049e9a: the XOR immediate is 0x8408.
 *
 *   CRC-16/CCITT (polynomial 0x1021, unreflected):
 *     Candidate for the cusotm vtable slot at offset 0x11c. Not yet
 *     confirmed from disassembly of that slot.
 *
 *   FCRC18 (altclass proprietary):
 *     A three-layer construction built on CRC-16/KERMIT with seed
 *     inversion and output byte-swap. Fully characterised from disassembly
 *     of FCRC18 at 0x8049edf. See altclass_crypto.h for the full description.
 *
 * Additional CRC variants present in the firmware (not implemented here):
 *
 *   MCRC16(unsigned long, unsigned long): in qcet, qpuso, Aqalpha.
 *     Takes two unsigned long arguments (not a buffer pointer), suggesting
 *     it operates on pre-assembled 32-bit data words.
 *
 *   RC16(unsigned short, unsigned short): in cicpcon, qptym, qpuso.
 *     Same two-argument pattern as MCRC16 but with 16-bit arguments.
 *
 *   FCRC16(char*, unsigned short): in qmir, qptym, Aqalpha.
 *     Standard buffer CRC with the FCRC naming convention.
 *
 *   FCRC(unsigned char*, unsigned short): in qcet, qpty.
 *     Base variant of the FCRC family.
 *
 *   FCRCM(unsigned char*, unsigned int): in qpuso.
 *     Variant with unsigned int length parameter.
 *
 *   FCRCM(and FCRC variants): in qpuso.
 *
 *   CheckCRC(unsigned char*, unsigned short): in cmdbf (stem300, mdbf80).
 *     Wraps CRC_Update (Modbus CRC16) for frame validation.
 *
 *   MakeCrc(unsigned char*, int): in libjevent.so.
 *     Journal record integrity CRC, algorithm unconfirmed.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>


/*
 * crc16_modbus
 *
 * Standard Modbus CRC16 with polynomial 0xa001 (reflected form of 0x8005).
 * Initial value: 0xffff.
 *
 * Confirmed used by sirius_mb, sirius_mb_ntu, mdbf, mdbtcp, and
 * stem300/cmdbf via the auchCRCHi and auchCRCLo static table symbols
 * and the exported CRC_Update(uint8_t*, uint16_t) function symbol.
 *
 * The firmware uses a lookup-table form (auchCRCHi and auchCRCLo), but
 * the lookup-table form and this bit-by-bit form produce identical output
 * for all inputs. The lookup tables are confirmed from the mangled symbol
 * names _ZL9auchCRCHi and _ZL9auchCRCLo in sirius_mb and cmdbf binaries.
 */
uint16_t crc16_modbus(const uint8_t *buf, size_t len);


/*
 * crc16_kermit_byte
 *
 * CRC-16/KERMIT single-byte accumulation step.
 * Polynomial: 0x8408 (reflected form of 0x1021).
 *
 * This is the inner crc16(unsigned char, unsigned short) function in the
 * altclass binary, confirmed from disassembly at 0x8049e9a:
 *   the XOR immediate in the bit loop is 0x8408.
 *
 * The initial value is passed in from outside, allowing the caller to
 * chain multiple calls to accumulate a CRC over a byte sequence.
 *
 * This function is the accumulator used inside FCRC18. It differs from
 * crc16_modbus in both polynomial and initialisation: Modbus uses 0xa001
 * with initial value 0xffff, while KERMIT uses 0x8408 with the initial
 * value fed from the FCRC18 seed construction.
 */
uint16_t crc16_kermit_byte(uint8_t byte, uint16_t crc_in);


/*
 * crc16_kermit
 *
 * CRC-16/KERMIT over a complete buffer.
 * Processes buf[0] through buf[len-1] via crc16_kermit_byte.
 * Initial value: 0x0000 (standard KERMIT initialisation).
 *
 * For use cases where an arbitrary initial value is required (as in FCRC18),
 * use crc16_kermit_byte directly with the desired seed.
 */
uint16_t crc16_kermit(const uint8_t *buf, size_t len);


/*
 * crc16_ccitt
 *
 * CRC16 CCITT with polynomial 0x1021 (unreflected).
 * Initial value: 0xffff.
 *
 * This is the alternative candidate for the cusotm vtable accumulator at
 * vtable offset 0x11c. It is commonly used in embedded serial protocols.
 * Pending confirmation via disassembly of the vtable slot.
 *
 * Note: this is a different algorithm from CRC-16/KERMIT despite both
 * using the 0x1021 polynomial. KERMIT uses the reflected polynomial
 * (0x8408) and processes bits LSB first; CCITT uses the unreflected
 * polynomial and processes bits MSB first.
 */
uint16_t crc16_ccitt(const uint8_t *buf, size_t len);


/*
 * usotm_frame_check
 *
 * Applies the TestPriem validation logic to a raw frame buffer, using
 * the provided CRC function as the accumulator.
 *
 * Implements the three-part check from cusotm::TestPriem at 0x804971a:
 *   1. Minimum length: len must be greater than 5.
 *   2. CRC check: computed over buf[1] through buf[len-3] inclusive,
 *      split into high and low bytes and compared against buf[len-2] and
 *      buf[len-1] respectively.
 *   3. Length consistency: buf[3] + 6 must equal len.
 *
 * The crc_fn parameter selects the candidate algorithm to test:
 *   pass crc16_modbus to test the Modbus variant
 *   pass crc16_ccitt to test the CCITT variant
 *
 * Note: buf[0] (the start byte) is excluded from the CRC computation.
 *   buf[len-2] and buf[len-1] (the CRC bytes) are also excluded.
 *
 * Returns 1 if all checks pass, 0 if any check fails.
 */
int usotm_frame_check(const uint8_t *buf, uint16_t len,
                      uint16_t (*crc_fn)(const uint8_t *, size_t));


/*
 * usotm_frame_check_kermit
 *
 * Variant of usotm_frame_check using the CRC-16/KERMIT accumulator.
 * Provided as a convenience for testing the altclass CRC in a USOTM
 * frame context, since both protocols share the same frame validation
 * structure (minimum length, CRC over buf[1..len-3], length consistency).
 *
 * The USOTM and altclass protocols likely share protocol heritage given
 * the overlapping type bytes (0x5a appears in both USOTM RaspakAnalog
 * and altclass GetDiscret), so testing KERMIT against USOTM frames
 * may yield useful information about the vtable slot algorithm.
 */
int usotm_frame_check_kermit(const uint8_t *buf, uint16_t len);
