/*
 * crc16.h
 *
 * CRC algorithm interface for USOTM and Modbus frame validation.
 *
 * Two candidate algorithms are provided. Disassembly of the cusotm vtable
 * slot at offset 0x11c is required to determine which one the firmware uses
 * for USOTM frame validation. The Modbus variant is confirmed used by
 * sirius_mb, sirius_mb_ntu, mdbf, mdbtcp, and cmdbf/stem300. The CCITT
 * variant is a candidate for the USOTM accumulator.
 *
 * See crc16-reconstructed.c for implementation notes and confidence levels.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>


/*
 * crc16_modbus
 *
 * Standard Modbus CRC16 using the polynomial 0xa001 (reflected form of
 * 0x8005). Confirmed used by sirius_mb, sirius_mb_ntu, mdbf, mdbtcp, and
 * stem300/cmdbf via the auchCRCHi and auchCRCLo symbol names and the
 * exported CRC_Update(uint8_t*, uint16_t) function symbol.
 *
 * Initial value: 0xffff.
 * Processes the input buffer byte by byte.
 * Returns a 16-bit CRC value.
 */
uint16_t crc16_modbus(const uint8_t *buf, size_t len);


/*
 * crc16_ccitt
 *
 * CRC16 CCITT using the polynomial 0x1021 (unreflected).
 *
 * Initial value: 0xffff.
 * Returns a 16-bit CRC value.
 *
 * This is the alternative candidate for the USOTM accumulator at vtable
 * offset 0x11c. It is commonly used in embedded serial protocols. Pending
 * confirmation via disassembly of the vtable slot.
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
 * Returns 1 if all checks pass, 0 if any check fails.
 *
 * Note: buf[0] (the start byte) is excluded from the CRC computation.
 *   buf[len-2] and buf[len-1] (the CRC bytes) are also excluded.
 */
int usotm_frame_check(const uint8_t *buf, uint16_t len,
                      uint16_t (*crc_fn)(const uint8_t *, size_t));
