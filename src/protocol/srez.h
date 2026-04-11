/*
 * srez.h
 *
 * RTOS link layer srez (snapshot) frame definitions.
 *
 * Recovered from disassembly of MakeSrezBuf(), MakeSrezWithFictiveTime(),
 * and MakeAdrBuf() in libkanaldrv.so.1.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>
#include "../structs/rtos_types.h"


/*
 * Magic start byte. Always 0x75 (ASCII u, from USO).
 * CONFIRMED from MakeSrezBuf disassembly.
 */
#define RTOS_SREZ_MAGIC         0x75


/*
 * Frame type discriminator values at header offset 7.
 * CONFIRMED from MakeSrezBuf disassembly.
 *
 * Note on RTOS_TYPE_FICTIVE: the value 0x82 is produced in the original
 * binary by a compiler-emitted SBB (subtract with borrow) instruction
 * sequence rather than a direct constant load. The arithmetic is
 * 0x75 minus 0x73 with the carry flag set, yielding 0x82. This is a
 * compiler artifact from the way the condition was written in the source
 * code, not a manually chosen magic value.
 */
#define RTOS_TYPE_FICTIVE       0x82  /* fictive timestamp, GPS unavailable */
#define RTOS_TYPE_NORMAL        0x7F  /* normal data buffer */
#define RTOS_TYPE_DISCRET       0x01  /* discrete sub-frame */
#define RTOS_TYPE_ANALOG        0x02  /* analog sub-frame */
#define RTOS_TYPE_IMPULS        0x09  /* impulse and counter sub-frame */


/*
 * Item strides in bytes per item of each type.
 * CONFIRMED from MakeSrezBuf loop analysis.
 */
#define RTOS_STRIDE_DISCRET     0x1C  /* 28 bytes per discrete item */
#define RTOS_STRIDE_ANALOG      0x68  /* 104 bytes per analog item */
#define RTOS_STRIDE_IMPULS      0x10  /* 16 bytes per impulse item */


/*
 * Byte offset of the status flag byte within each item stride.
 * CONFIRMED from MakeSrezBuf analysis.
 *
 * The flag byte indicates whether the data in this item is fresh (newly
 * received from the field) or stale (carried over from a previous cycle).
 * The named bit positions within each flag byte:
 *   Discrete:  bit 1 (0x02) = data is fresh
 *   Analog:    bit 2 (0x04) = data is fresh
 *   Impulse:   bit 0 (0x01) = data is fresh
 */
#define RTOS_FLAG_DISCRET_OFFSET  0x1B
#define RTOS_FLAG_ANALOG_OFFSET   0x61
#define RTOS_FLAG_IMPULS_OFFSET   0x0C

#define RTOS_FLAG_DISCRET_FRESH   0x02
#define RTOS_FLAG_ANALOG_FRESH    0x04
#define RTOS_FLAG_IMPULS_FRESH    0x01


/*
 * rtos_srez_frame
 *
 * Complete srez frame layout: a 10-byte header followed by a variable-length
 * payload of typed I/O items. The item count is carried in the SOST_SEND
 * struct field at offset +0x2, written by MakeSrezBuf alongside the header.
 * Items are laid out contiguously using the RTOS_STRIDE_* values above.
 */
struct rtos_srez_frame {
    RTOS_SREZ_HEADER header;    /* 10 bytes */
    uint8_t          payload[]; /* N items, stride and encoding by type byte */
};


/*
 * rtos_srez_build_header
 *
 * Build a srez frame header in a caller-supplied 10-byte buffer.
 * The buffer is first zeroed, then populated with the magic byte,
 * BCD timestamp, and type discriminator. The reserved bytes 8 and 9
 * remain zero as required by the protocol.
 *
 * buf:  output buffer, must be at least 10 bytes
 * type: one of the RTOS_TYPE_* constants above
 * ts:   6-byte BCD timestamp in TIME_SERVER_KANAL format
 */
static inline void
rtos_srez_build_header(uint8_t *buf, uint8_t type,
                       const TIME_SERVER_KANAL *ts)
{
    int i;
    for (i = 0; i < 10; i++)
        buf[i] = 0;

    buf[0] = RTOS_SREZ_MAGIC;
    buf[1] = ts->year;
    buf[2] = ts->month;
    buf[3] = ts->day;
    buf[4] = ts->hour;
    buf[5] = ts->minute;
    buf[6] = ts->second;
    buf[7] = type;
    /* bytes 8 and 9 remain 0x0000 */
}


/*
 * rtos_item_count
 *
 * Compute the number of items in a srez frame payload given the total
 * payload length and the type byte. Returns 0 for unknown type bytes.
 */
static inline uint32_t
rtos_item_count(uint8_t type, uint16_t payload_len)
{
    switch (type) {
    case RTOS_TYPE_DISCRET:
        return payload_len / RTOS_STRIDE_DISCRET;
    case RTOS_TYPE_ANALOG:
        return payload_len / RTOS_STRIDE_ANALOG;
    case RTOS_TYPE_IMPULS:
        return payload_len / RTOS_STRIDE_IMPULS;
    default:
        return 0;
    }
}
