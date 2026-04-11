/*
 * crc16.h
 *
 * Public interface for the reconstructed CRC algorithms used in the firmware.
 *
 * Three CRC variants are present across the firmware binaries.
 *
 * crc16_modbus uses polynomial 0xa001, the reflected form of 0x8005, with
 * initial value 0xffff. Confirmed used by sirius_mb, sirius_mb_ntu, mdbf,
 * mdbf80, mdbtcp, and stem300/cmdbf via the auchCRCHi and auchCRCLo static
 * table symbols and the exported CRC_Update symbol.
 *
 * crc16_ccitt uses polynomial 0x1021 unreflected with initial value 0xffff.
 * This is the candidate for the cusotm vtable accumulator at offset 0x11c
 * called by TestPriem. Not yet confirmed by direct disassembly of that slot.
 *
 * crc16_kermit_byte is the inner accumulator for the FCRC18 algorithm used
 * by the altclass driver in progr/qalfat. Polynomial 0x8408, the reflected
 * form of 0x1021. No fixed initial value; the caller supplies the running
 * CRC. Confirmed from disassembly of crc16() at 0x8049e9a in qalfat.
 *
 * fcrc18 is the complete frame integrity algorithm for the altclass protocol.
 * It wraps crc16_kermit_byte with a seed derived from the bitwise complement
 * and byte-swap of the first two buffer bytes, feeds all remaining bytes
 * through the accumulator, then feeds two zero bytes, and finally applies
 * bitwise NOT and byte-swap to the result. Confirmed from disassembly of
 * FCRC18 at 0x8049edf in qalfat.
 *
 * usotm_frame_check applies the three-part TestPriem validation logic:
 * minimum length greater than five bytes, CRC over buf[1] through buf[len-3],
 * and the length consistency check buf[3] plus 6 equals total length.
 * Confirmed from cusotm::TestPriem at 0x804971a in progr/usotm.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>


uint16_t crc16_modbus(const uint8_t *buf, size_t len);
uint16_t crc16_ccitt(const uint8_t *buf, size_t len);
uint16_t crc16_kermit_byte(uint8_t byte, uint16_t crc_in);
uint16_t fcrc18(const uint8_t *buf, uint16_t len);
int      usotm_frame_check(const uint8_t *buf, uint16_t len,
                            uint16_t (*crc_fn)(const uint8_t *, size_t));
