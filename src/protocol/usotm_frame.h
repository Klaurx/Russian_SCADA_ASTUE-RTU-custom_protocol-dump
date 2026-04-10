/*
 * usotm_frame.h
 *
 * USOTM field bus frame layout definitions.
 *
 * Recovered from disassembly of TestPriem, RaspakDiscret, RaspakAddAnalog,
 * RaspakImpuls, RaspakTempVn, RaspakTempNar, RaspakAnalog, RaspakOldAnalog,
 * AddKod, SetGroupTu, SendTuCommand, and SendTuFromQuery in the usotm binary
 * (cusotm class).
 *
 * Confidence markers:
 *   CONFIRMED   verified directly from disassembly byte offset accesses
 *   INFERRED    derived from frame length formula and context
 *   PENDING     requires direct disassembly of IsFirstByte or the vtable slot
 *
 * Corrections from earlier documentation:
 *   The analog response type byte is 0x5a, not 0x86. The 0x86 value was
 *   an error. The corrected value is confirmed from RaspakAnalog at 0x804ad62:
 *     cmp BYTE PTR [edi+0x1], 0x5a
 *
 *   The thread synchronisation path using mtx and cond is in RaspakImpuls,
 *   not in RaspakAddAnalog. The analog parser does not use mutex/condition
 *   synchronisation.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>


/*
 * Minimum valid frame length.
 * CONFIRMED from TestPriem: cmp di, 0x5 / jbe invalid
 */
#define USOTM_MIN_FRAME_LEN     6


/*
 * Total frame overhead (header plus CRC).
 * Derived from the confirmed length formula: total_len = N + 6
 *   where N is the payload item count byte at buf[3].
 */
#define USOTM_OVERHEAD          6


/*
 * Response type bytes at frame offset 1.
 *
 * CONFIRMED from type checks in each Raspak function:
 *
 *   0x57 confirmed at RaspakDiscret 0x804a3cf:
 *     cmp BYTE PTR [ecx+0x1], 0x57
 *
 *   0x56 confirmed at RaspakImpuls 0x804b2f4:
 *     cmp BYTE PTR [eax+0x1], 0x56
 *
 *   0x5a confirmed at RaspakAnalog 0x804ada8:
 *     cmp BYTE PTR [edi+0x1], 0x5a
 *
 *   0x5d confirmed at RaspakTempVn (type check in that function's preamble)
 *
 * PENDING: type bytes for RaspakOldAnalog, RaspakAddAnalog, RaspakTempNar
 *   require further disassembly.
 */
#define USOTM_TYPE_DISCRET      0x57  /* discrete poll response */
#define USOTM_TYPE_IMPULS       0x56  /* impulse counter response */
#define USOTM_TYPE_ANALOG       0x5A  /* new-format analog response, 32 channels */
#define USOTM_TYPE_TEMPVN       0x5D  /* internal temperature response */
/* USOTM_TYPE_TEMPNAR: pending disassembly of RaspakTempNar */
/* USOTM_TYPE_OLDANALOG: pending disassembly of RaspakOldAnalog */
/* USOTM_TYPE_ADDANALOG: pending disassembly of RaspakAddAnalog */


/*
 * Start byte for outbound frames.
 *
 * INFERRED from outbound frame construction in SetGroupTu, SendZaprosUkd,
 * and SendTuCommand. All observed outbound frames store 0x5b at byte 0.
 * IsFirstByte at 0x804970a likely validates 0x5b for inbound frames as well,
 * since field devices respond with the same framing scheme. Direct disassembly
 * of IsFirstByte is required for confirmation.
 */
#define USOTM_START_BYTE        0x5B  /* INFERRED, pending IsFirstByte disassembly */


/*
 * usotm_frame_header
 *
 * First 4 bytes of every USOTM frame, common to all response types.
 *
 * Byte 0: start byte, validated by IsFirstByte before TestPriem is called.
 * Byte 1: response type byte, identifies the Raspak function to call.
 * Byte 2: device address, compared against tag[+0x0f] in every parser.
 * Byte 3: payload item count N, used in TestPriem length check.
 */
struct usotm_frame_header {
    uint8_t  start;          /* INFERRED: start byte, likely 0x5b */
    uint8_t  type;           /* CONFIRMED: response type byte */
    uint8_t  device_addr;    /* CONFIRMED: compared to tag[+0x0f] */
    uint8_t  item_count;     /* CONFIRMED: N in total_len = N + 6 */
};


/*
 * usotm_frame_trailer
 *
 * Last 2 bytes of every frame: the CRC, high byte first.
 * CONFIRMED from TestPriem CRC byte comparisons.
 */
struct usotm_frame_trailer {
    uint8_t  crc_hi;         /* CONFIRMED: (crc >> 8) & 0xff */
    uint8_t  crc_lo;         /* CONFIRMED: crc & 0xff */
};


/*
 * usotm_discret_payload
 *
 * Discrete response payload (type 0x57).
 *
 * The payload item count N at buf[3] is scaled to get the word count:
 *   word_count = ((N * 0xab) >> 8) >> 2
 * This formula is CONFIRMED from RaspakDiscret disassembly.
 *
 * Each status word is assembled big-endian from two consecutive bytes:
 *   high byte at buf[4 + i*2], low byte at buf[5 + i*2].
 *
 * Tag offsets written by RaspakDiscret (CONFIRMED):
 *   tag[+0xbc]          OR'd with 0x02 (fresh), 0x10, 0x04
 *   tag[+0x04 + i*2]    current value word at base offset 0x50
 *   tag[+0x0c + i*2]    mask word
 *   tag[+0xc8 + i*2]    previous value word for change detection
 *   tag[+0xb4 + i*2]    received value word
 *
 * After successful parse, SendSbrosLatch(uso_index) is called (CONFIRMED).
 */
struct usotm_discret_payload {
    uint8_t status_words[]; /* N raw bytes, big-endian uint16 pairs */
};


/*
 * usotm_impuls_payload
 *
 * Impulse counter response payload (type 0x56).
 *
 * Bytes at offsets 0 through 3 form a 32-bit bitmask covering 32 counter
 * positions. The bit count per byte is computed by calling KolBits on each
 * byte separately (CONFIRMED: four separate KolBits calls on bytes 4,5,6,7
 * in RaspakImpuls).
 *
 * Expected frame length: (total_active_counters * 2) + 10
 *   (4 header + 4 bitmask + data + 2 CRC)
 *
 * Counter values follow the bitmask starting at payload byte 4, with one
 * big-endian 16-bit value per set bit, advancing by 2 per active counter.
 *
 * Thread synchronisation: after length validation, RaspakImpuls acquires
 * the global mutex mtx at 0x804eec8 and signals the consumer via cond at
 * 0x804eed0. Both are named globals (symbol type D, confirmed from nm).
 * This is in RaspakImpuls, not in any analog parser.
 */
struct usotm_impuls_payload {
    uint8_t bitmask[4];  /* 32 bits, which counters are present */
    uint8_t values[];    /* big-endian 16-bit values, one per set bit */
};


/*
 * usotm_tempvn_payload
 *
 * Internal temperature response payload (type 0x5d).
 *
 * CONFIRMED from RaspakTempVn: frame length must be exactly 8 bytes
 * (the check is si == 0x8), meaning item_count N at buf[3] must be 2.
 *
 * Temperature is assembled as a signed 16-bit integer:
 *   value = (temp_hi << 8) | temp_lo
 * Stored at tag[+0x1b4] as int32. Fresh flag at tag[+0x1be] set to 1.
 */
struct usotm_tempvn_payload {
    uint8_t temp_hi;  /* high byte of signed 16-bit temperature */
    uint8_t temp_lo;  /* low byte of signed 16-bit temperature */
};


/*
 * usotm_analog_payload
 *
 * New-format analog response payload (type 0x5a, RaspakAnalog).
 *
 * Bytes 0 through 3 form a 32-bit bitmask (same structure as impulse).
 * KolBits is called on each of the 4 bitmask bytes separately.
 * Expected length: (total_active_channels * 2) + 0x0a
 * Loop bound: 32 channels (cmp ebx, 0x20 at 0x804aef2).
 * For each active channel, AddKod(uso_index, channel, value) is called.
 *
 * CONFIRMED: type byte 0x5a at 0x804ada8, loop to 32 at 0x804aef2.
 */
struct usotm_analog_new_payload {
    uint8_t bitmask[4];  /* 32 bits, which channels are present */
    uint8_t values[];    /* big-endian 16-bit channel values, one per set bit */
};


/*
 * usotm_analog_old_payload
 *
 * Old-format analog response payload (RaspakOldAnalog).
 *
 * Loop bound: 8 channels (cmp ebx, 0x8 at 0x804ad3b).
 * Also calls AddKod for each active channel.
 * Type byte: PENDING disassembly.
 * Used as a fallback when the new-format RaspakAnalog fails.
 */
struct usotm_analog_old_payload {
    uint8_t data[]; /* raw analog data, up to 8 channels */
};


/*
 * usotm_setgrouptu_frame
 *
 * Outbound group telecontrol command frame built by SetGroupTu().
 * Total length: 18 bytes (0x12).
 * CONFIRMED from disassembly showing 18-byte SendBuffer call.
 */
struct usotm_setgrouptu_frame {
    uint8_t start;           /* 0x5b, CONFIRMED from mov BYTE PTR, 0x5b */
    uint8_t cmd_type;        /* 0x04, group telecontrol command */
    uint8_t device_addr;     /* tag[+0x39] */
    uint8_t reserved0;       /* 0x00 */
    uint8_t device_id[4];    /* tag[+0x4] through tag[+0x7] */
    uint8_t reserved1;       /* 0x00 */
    uint8_t reserved2;       /* 0x00 */
    uint8_t bitmask_enc[4];  /* NOT(mask) << 16 | mask, 4 bytes */
    uint8_t activate;        /* 0x01 to activate, 0x00 to deactivate */
    uint8_t pad[3];          /* padding to 18 bytes */
};


/*
 * USOTM_SENDTU_LEN
 *
 * Length of the SendTuCommand outbound frame.
 * CONFIRMED from disassembly: mov DWORD PTR [esp+0x8], 0x7 before SendBuffer.
 * This is a distinct, shorter command from SetGroupTu (18 bytes).
 */
#define USOTM_SENDTU_LEN        7


/*
 * usotm_tu_queue_record
 *
 * One record in the TU command queue drained by SendTuFromQuery().
 * Records are stored contiguously starting at object offset +0x4e28.
 * Queue count is at object offset +0x5080.
 * CONFIRMED from disassembly of SendTuFromQuery at 0x804c290.
 */
struct usotm_tu_queue_record {
    uint16_t param_a;  /* +0x4e28, word arg2 to SendTuCommand */
    uint16_t param_b;  /* +0x4e2a, word arg4 to SendTuCommand */
    uint8_t  flag_a;   /* +0x4e2c, byte arg1 to SendTuCommand */
    uint8_t  flag_b;   /* +0x4e2d, byte arg3 to SendTuCommand */
};


/*
 * USOTM_TU_QUEUE_OFFSET and USOTM_TU_COUNT_OFFSET
 *
 * Object offsets for the TU command queue in the cusotm instance.
 * CONFIRMED from SendTuFromQuery disassembly.
 */
#define USOTM_TU_QUEUE_OFFSET   0x4E28  /* start of 6-byte queue records */
#define USOTM_TU_COUNT_OFFSET   0x5080  /* queue item count, reset to 0 after drain */
