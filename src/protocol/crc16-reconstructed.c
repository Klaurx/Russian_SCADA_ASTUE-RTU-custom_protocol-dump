/*
 * crc16.c
 *
 * Reconstructed CRC algorithm for the USOTM field bus protocol,
 * as used in cusotm::TestPriem (usotm binary, 0x804971A).
 *
 * The CRC covers bytes 1 through len-3 inclusive (skipping the start byte
 * and the two CRC bytes at the frame tail). The accumulation function is
 * a vtable call at [ecx+0x11C] in the original binary, so the exact
 * polynomial is not directly confirmed from disassembly of TestPriem alone.
 *
 * The result is split into high and low bytes and compared against
 * buf[len-2] and buf[len-1].
 *
 * Two candidate algorithms are implemented below. The Modbus CRC16 with
 * polynomial 0xA001 (reflected 0x8005) is confirmed for sirius_mb and mdbf
 * via the auchCRCHi/auchCRCLo symbol names. Whether cusotm uses the same
 * polynomial or a different one (e.g. CRC16 CCITT 0x1021) is pending
 * disassembly of the vtable slot at [ecx+0x11C].
 *
 * Authorization: view-only research license. See LICENSE.
 */

#include "crc16.h"
#include <stdint.h>
#include <stddef.h>

/*
 * Standard Modbus CRC16 (polynomial 0xA001, reflected 0x8005).
 * Confirmed used by sirius_mb and mdbf via modbusCRC.cc symbols.
 * Candidate for USOTM accumulator.
 */
uint16_t crc16_modbus(const uint8_t *buf, size_t len)
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
 * CRC16 CCITT (polynomial 0x1021, unreflected).
 * Alternative candidate for USOTM accumulator.
 */
uint16_t crc16_ccitt(const uint8_t *buf, size_t len)
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
 * Apply the TestPriem validation logic to a raw frame buffer.
 * Returns 1 if the frame passes all checks, 0 otherwise.
 *
 * The crc_fn parameter allows testing both candidate algorithms.
 * Pass crc16_modbus or crc16_ccitt.
 *
 * Note: the start byte (buf[0]) is not included in the CRC range,
 * and neither are the two trailing CRC bytes.
 */
int usotm_frame_check(const uint8_t *buf, uint16_t len,
                      uint16_t (*crc_fn)(const uint8_t *, size_t))
{
    uint16_t computed, payload_count;

    if (len <= 5)
        return 0;

    /* CRC covers buf[1] through buf[len-3] */
    computed = crc_fn(buf + 1, len - 3);

    if (((computed >> 8) & 0xFF) != buf[len - 2])
        return 0;

    if ((computed & 0xFF) != buf[len - 1])
        return 0;

    /* Length consistency: total = N + 6 */
    payload_count = buf[3];
    if ((payload_count + 6) != len)
        return 0;

    return 1;
}
