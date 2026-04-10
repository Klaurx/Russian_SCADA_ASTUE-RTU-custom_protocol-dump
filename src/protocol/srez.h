/*
 * srez.h
 *
 * RTOS link layer srez (snapshot) frame definitions.
 * Recovered from MakeSrezBuf, MakeSrezWithFictiveTime, MakeAdrBuf
 * disassembly of libkanaldrv.so.1.
 */

#pragma once
#include <stdint.h>
#include "../structs/rtos_types.h"

/* Magic start byte. Always 0x75 (ASCII 'u', from USO). CONFIRMED. */
#define RTOS_SREZ_MAGIC      0x75

/* Frame type discriminator values. CONFIRMED from MakeSrezBuf. */
#define RTOS_TYPE_FICTIVE    0x82  /* fictive timestamp (synthesised) */
#define RTOS_TYPE_NORMAL     0x7F  /* normal data buffer */
#define RTOS_TYPE_DISCRET    0x01  /* discrete sub-frame */
#define RTOS_TYPE_ANALOG     0x02  /* analog sub-frame */
#define RTOS_TYPE_IMPULS     0x09  /* impulse/counter sub-frame */

/* Item strides in bytes. CONFIRMED from MakeSrezBuf loop analysis. */
#define RTOS_STRIDE_DISCRET  0x1C  /* 28 bytes per discrete item */
#define RTOS_STRIDE_ANALOG   0x68  /* 104 bytes per analog item */
#define RTOS_STRIDE_IMPULS   0x10  /* 16 bytes per impulse item */

/* Offset of status flag byte within each item stride. CONFIRMED. */
#define RTOS_FLAG_DISCRET_OFFSET  0x1B  /* bit 1 = data fresh */
#define RTOS_FLAG_ANALOG_OFFSET   0x61  /* bit 2 = data fresh */
#define RTOS_FLAG_IMPULS_OFFSET   0x0C  /* bit 0 = data fresh */

/*
 * Complete srez frame structure.
 *
 * The header is 10 bytes followed by a variable-length payload of typed
 * I/O items. The SOST_SEND.sequence field (at sost_send+0x2) holds the
 * item count written by MakeSrezBuf into the buffer alongside the header.
 */
struct rtos_srez_frame {
    RTOS_SREZ_HEADER header;    /* 10 bytes: magic + timestamp + type + reserved */
    uint8_t          payload[]; /* N items, stride and interpretation by type byte */
};

/*
 * Build a srez header in a caller-supplied buffer.
 * buf must be at least 10 bytes.
 * type is one of the RTOS_TYPE_* constants.
 * ts is the 6-byte BCD timestamp.
 */
static inline void rtos_srez_build_header(uint8_t *buf, uint8_t type,
                                           const TIME_SERVER_KANAL *ts)
{
    int i;
    for (i = 0; i < 10; i++) buf[i] = 0;
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
