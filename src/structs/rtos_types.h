/**
 * rtos_types.h
 *
 * Reconstructed RTOS protocol type definitions.
 * Sources: libkanaldrv.so.1 disassembly (Obmen, AnalizBufPriemAndSendOtvet,
 *          MakeSrezBuf, MakeSrezWithFictiveTime, MakeAdrBuf, InitSend,
 *          AnalizBufInitPriem), constructor at 0x6086, RaspakKeys at 0x66DE.
 * Compiler: GCC 4.3.3, QNX Neutrino i386.
 *
 * Confidence markers:
 *   CONFIRMED   offset and type verified directly from disassembly
 *   INFERRED    derived from usage context and function signatures
 *   ESTIMATED   size known, internals guessed from field naming
 */

#pragma once
#include <stdint.h>

/**
 * RTOS_RETRANSLATE_ADR
 *
 * Network address block for the RTOS retranslation layer.
 * Recovered from AnalizBufPriemAndSendOtvet command 0x04 handler which
 * unpacks fields at offsets +0x0, +0x4, +0x8, +0xC into the handshake
 * save area at obj+0x17004..0x1700E.
 *
 * CONFIRMED: four uint16 fields at 4-byte aligned offsets.
 * ESTIMATED: padding bytes between fields.
 */
struct RTOS_RETRANSLATE_ADR {
    uint16_t channel_id;     /* +0x0  CONFIRMED: extracted to [obj+0x17006] */
    uint16_t _pad0;
    uint16_t secondary_adr;  /* +0x4  CONFIRMED: extracted to [obj+0x1700C] */
    uint16_t _pad1;
    uint16_t primary_adr;    /* +0x8  CONFIRMED: extracted to [obj+0x17004] */
    uint16_t _pad2;
    uint16_t tertiary_adr;   /* +0xC  CONFIRMED: extracted to [obj+0x1700A] */
    uint16_t _pad3;
};

/**
 * SOST_PRIEM
 *
 * Receive channel state. Used by PriemPacket, AnalizBufPriem, SendKvitok.
 * CONFIRMED: size 0x12 (18 bytes) from constructor zeroing at obj+0x1D0.
 * ESTIMATED: internal field layout from usage pattern.
 */
struct SOST_PRIEM {
    uint8_t  rx_state;       /* +0x0  FSM state */
    uint8_t  flags;          /* +0x1  receive flags */
    uint16_t rx_count;       /* +0x2  bytes accumulated so far */
    uint16_t expected;       /* +0x4  expected total length */
    uint16_t error_count;    /* +0x6  frame error counter */
    uint8_t  last_byte;      /* +0x8  last byte received */
    uint8_t  _pad[9];        /* +0x9  pad to 18 bytes */
};

/**
 * SOST_SEND
 *
 * Transmit channel state. Three instances in kanaldrv object:
 *   sost_send1 at obj+0x1F4 (8 bytes, base variant)
 *   sost_send2 at obj+0x1FC (18 bytes, extended variant)
 *   sost_send3 at obj+0x20E (8 bytes, base variant)
 *
 * CONFIRMED: field at [+0x2] is the sequence number, extracted in command
 * 0x04 handler and in initkandrv::InitSend which tests [sost+0x2] != 0
 * before transmitting.
 */
struct SOST_SEND {
    uint8_t  state;          /* +0x0  transmit state */
    uint8_t  flags;          /* +0x1  control flags */
    uint16_t sequence;       /* +0x2  CONFIRMED: sequence/frame count */
    uint32_t last_tx_time;   /* +0x4  timestamp of last transmission */
};

/**
 * SOST_TIME_CORRECT
 *
 * Time correction state for GPS synchronisation.
 * CONFIRMED: 16 bytes at obj+0x1E4, passed to AnalizBufPriem.
 * Pointer to this struct passed to IsTimeCorrectAllow.
 * ESTIMATED: internal layout.
 */
struct SOST_TIME_CORRECT {
    int32_t  correction_ms;  /* +0x0  time offset from GPS reference */
    uint8_t  enabled;        /* +0x4  correction enabled flag */
    uint8_t  valid;          /* +0x5  correction valid flag */
    uint16_t _pad;
    uint32_t last_sync_ms;   /* +0x8  timestamp of last GPS sync */
    uint16_t drift_ppb;      /* +0xC  measured drift in ppb */
    uint16_t _pad2;
};

/**
 * TIME_SERVER_KANAL
 *
 * 6-byte timestamp as embedded in RTOS srez frames.
 * CONFIRMED: 6 bytes copied by MakeSrezWithFictiveTime into frame at offset 1.
 * Format: BCD encoded, most likely YY MM DD HH MM SS.
 * Confirmed BCD from GPS time conversion functions in qcet (ConvLocToSerKan).
 */
struct TIME_SERVER_KANAL {
    uint8_t year;    /* BCD, e.g. 0x24 for 2024 */
    uint8_t month;   /* BCD, 01..12 */
    uint8_t day;     /* BCD, 01..31 */
    uint8_t hour;    /* BCD, 00..23 */
    uint8_t minute;  /* BCD, 00..59 */
    uint8_t second;  /* BCD, 00..59 */
};

/**
 * TIME_SERVER
 *
 * Full GPS time server structure.
 * Used by usodrv::GetLocalTime(TIME_SERVER*, SYSTEMTIME*, long).
 * Exact layout unknown. Passed from GPS shared memory segment.
 * ESTIMATED: 32 bytes based on typical GPS time structures.
 */
struct TIME_SERVER {
    uint8_t raw[32]; /* contents unknown, mapped from GPS SHM */
};

/**
 * SYSTEMTIME
 *
 * Windows-compatible SYSTEMTIME structure used as output from GetLocalTime.
 * Standard layout, 16 bytes.
 */
struct SYSTEMTIME {
    uint16_t wYear;
    uint16_t wMonth;
    uint16_t wDayOfWeek;
    uint16_t wDay;
    uint16_t wHour;
    uint16_t wMinute;
    uint16_t wSecond;
    uint16_t wMilliseconds;
};

/**
 * Srez frame header (10 bytes)
 *
 * Laid out inline in the TX buffer. Not a named struct in the source,
 * but implied by MakeSrezBuf and MakeSrezWithFictiveTime.
 *
 * CONFIRMED: magic 0x75 at offset 0.
 * CONFIRMED: timestamp 6 bytes at offsets 1..6.
 * CONFIRMED: type byte at offset 7 (0x82, 0x7F, etc).
 * CONFIRMED: 2 zero bytes at offsets 8..9.
 */
struct RTOS_SREZ_HEADER {
    uint8_t             magic;      /* always 0x75 */
    TIME_SERVER_KANAL   timestamp;  /* 6 bytes BCD */
    uint8_t             type;       /* frame type discriminator */
    uint16_t            reserved;   /* always 0x0000 */
};

/**
 * SUPPORTED_USO_AND_BUF_TYPES
 *
 * Capability bitmap exchanged during driver initialisation.
 * Used by usodrv::SetSupportTypes and GetSupportTypes.
 * ESTIMATED: two 32-bit bitmasks based on name semantics.
 */
struct SUPPORTED_USO_AND_BUF_TYPES {
    uint32_t uso_type_mask; /* bitmask of supported USO device types */
    uint32_t buf_type_mask; /* bitmask of supported buffer types */
};
