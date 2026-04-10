/*
 * usotm_frame.h
 *
 * USOTM field bus frame layout definitions.
 * Recovered from TestPriem, RaspakDiscret, RaspakAddAnalog, RaspakImpuls,
 * RaspakTempVn disassembly (usotm binary, cusotm class).
 *
 * Confidence markers:
 *   CONFIRMED   verified directly from disassembly byte offset accesses
 *   INFERRED    derived from frame length formula and context
 *   UNKNOWN     placeholder, requires further disassembly
 */

#pragma once
#include <stdint.h>

/* Minimum valid USOTM frame length (from TestPriem: len <= 5 returns invalid) */
#define USOTM_MIN_FRAME_LEN  6

/* Length formula: total_len = payload_item_count + 6 */
#define USOTM_OVERHEAD       6

/*
 * Response type bytes at frame offset 1.
 * CONFIRMED from type checks in each Raspak* function.
 */
#define USOTM_TYPE_DISCRET   0x57
#define USOTM_TYPE_IMPULS    0x56
#define USOTM_TYPE_TEMPVN    0x5D
#define USOTM_TYPE_ANALOG    0x86

/*
 * USOTM frame header (first 4 bytes before variable payload).
 *
 * Offset 0: start byte, validated by IsFirstByte() before TestPriem.
 *           Exact value unknown pending IsFirstByte disassembly.
 * Offset 1: response type byte (USOTM_TYPE_*)
 * Offset 2: device address, compared against tag[+0x0F] in all parsers.
 * Offset 3: payload item count N, used in TestPriem length check and
 *           in RaspakDiscret to derive the number of status words.
 */
struct usotm_frame_header {
    uint8_t  start;          /* UNKNOWN: magic start byte for IsFirstByte */
    uint8_t  type;           /* CONFIRMED: response type byte */
    uint8_t  device_addr;    /* CONFIRMED: compared to tag[+0x0F] */
    uint8_t  item_count;     /* CONFIRMED: N in len = N + 6 */
};

/*
 * USOTM frame trailer (last 2 bytes of every frame).
 */
struct usotm_frame_trailer {
    uint8_t  crc_hi;         /* CONFIRMED: (crc >> 8) & 0xFF */
    uint8_t  crc_lo;         /* CONFIRMED: crc & 0xFF */
};

/*
 * Discrete response payload layout.
 *
 * buf[3] * 0xAB >> 8 >> 2 gives the number of 16-bit status words.
 * Each word is assembled big-endian from two consecutive bytes.
 * Words are at buf[4 + i*2] (high) and buf[5 + i*2] (low).
 * CONFIRMED from RaspakDiscret disassembly.
 */
struct usotm_discret_payload {
    uint8_t status_words[];  /* N pairs of bytes, big-endian uint16 each */
};

/*
 * Impulse counter response payload layout.
 *
 * Bytes 4..7 are 4 bitmask bytes covering 32 bit positions (counters 0..31).
 * For each set bit position, one 16-bit counter value appears in the
 * variable-length payload region starting at buf[8], advancing by 2 per
 * active counter. CONFIRMED from RaspakImpuls disassembly.
 */
struct usotm_impuls_payload {
    uint8_t  bitmask[4];    /* 32 bits: which counters are present */
    uint8_t  values[];      /* 16-bit counter values, one per set bit */
};

/*
 * Internal temperature response payload.
 *
 * Only valid when frame length is exactly 8 (item_count = 2).
 * Temperature at buf[4] (high) and buf[5] (low), signed 16-bit.
 * Stored as int32 in tag[+0x1B4]. CONFIRMED from RaspakTempVn.
 */
struct usotm_tempvn_payload {
    uint8_t  temp_hi;
    uint8_t  temp_lo;
};

/*
 * Analog response payload.
 *
 * Item count at buf[3], maximum 0x37 (55) channels.
 * When count <= 55, payload is moved directly via memmove into the device
 * tag structure at offset +0x1C6. The count is stored at tag[+0x1FE].
 * When count > 55, a thread synchronisation path is used (producer/consumer
 * with global mutex at 0x804EEC8 and condition at 0x804EED0).
 * CONFIRMED from RaspakAddAnalog disassembly.
 *
 * Internal encoding of individual analog values within the payload is
 * unknown pending further analysis of the tag structure at +0x1C6.
 */
struct usotm_analog_payload {
    uint8_t  data[];         /* raw bytes moved into tag[+0x1C6] */
};
