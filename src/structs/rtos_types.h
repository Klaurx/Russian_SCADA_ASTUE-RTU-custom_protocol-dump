/*
 * rtos_types.h
 *
 * Reconstructed RTOS protocol type definitions for the libkanaldrv.so.1
 * channel driver and related components.
 *
 * Sources: disassembly of Obmen, AnalizBufPriemAndSendOtvet, MakeSrezBuf,
 *   MakeSrezWithFictiveTime, MakeAdrBuf, InitSend, AnalizBufInitPriem in
 *   libkanaldrv.so.1. Constructor at offset 0x6086. RaspakKeys at 0x66de.
 *   libsystypes.so.1 symbol table including _MakeKvitok, _PriemPacket,
 *   _AnalizBufPriem, _AnalizPriem, _MakeNextPachka, _PrepareBufRetr.
 *
 * Compiler: GCC 4.3.3, QNX Neutrino i386.
 *
 * Confidence markers used throughout this file:
 *   CONFIRMED   offset and type verified directly from disassembly byte accesses
 *   INFERRED    derived from usage context and function signatures
 *   ESTIMATED   size is known, internal layout guessed from field naming
 *   PENDING     requires additional disassembly to confirm
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>


/*
 * RTOS_RETRANSLATE_ADR
 *
 * Network address block for the RTOS retranslation layer. Recovered from
 * the command 0x04 handler in AnalizBufPriemAndSendOtvet, which unpacks
 * fields at offsets +0x0, +0x4, +0x8, and +0xc into the handshake save
 * area at obj+0x17004 through obj+0x1700e.
 *
 * CONFIRMED: four uint16 fields at 4-byte aligned offsets within the struct.
 * ESTIMATED: padding bytes between fields (natural alignment for i386).
 */
struct RTOS_RETRANSLATE_ADR {
    uint16_t channel_id;     /* +0x0  CONFIRMED: extracted to [obj+0x17006] */
    uint16_t pad0;
    uint16_t secondary_adr;  /* +0x4  CONFIRMED: extracted to [obj+0x1700c] */
    uint16_t pad1;
    uint16_t primary_adr;    /* +0x8  CONFIRMED: extracted to [obj+0x17004] */
    uint16_t pad2;
    uint16_t tertiary_adr;   /* +0xc  CONFIRMED: extracted to [obj+0x1700a] */
    uint16_t pad3;
};


/*
 * SOST_PRIEM
 *
 * Receive channel state machine. Used by PriemPacket (via _PriemPacket in
 * libsystypes.so.1), AnalizBufPriem, and SendKvitok. A SOST_PRIEM instance
 * lives at kanaldrv object offset +0x1d0.
 *
 * CONFIRMED: total size 0x12 bytes (18) from the constructor zeroing sequence
 *   which zeroes exactly 18 bytes at obj+0x1d0.
 * ESTIMATED: internal field layout derived from usage in _PriemPacket and
 *   from the context of how AnalizBufPriem returns partial/complete/error.
 * PENDING: exact byte offsets of rx_count, error_count require disassembly
 *   of _PriemPacket and _AnalizBufPriem in libsystypes.so.1.
 */
struct SOST_PRIEM {
    uint8_t  rx_state;       /* +0x0  FSM state: idle, accumulating, complete */
    uint8_t  flags;          /* +0x1  receive control flags */
    uint16_t rx_count;       /* +0x2  INFERRED: bytes accumulated so far */
    uint16_t expected_len;   /* +0x4  INFERRED: expected total frame length */
    uint16_t error_count;    /* +0x6  INFERRED: frame error counter */
    uint8_t  last_byte;      /* +0x8  INFERRED: most recently received byte */
    uint8_t  pad[9];         /* +0x9  padding to reach total size of 18 bytes */
};


/*
 * SOST_SEND
 *
 * Transmit channel state. Three instances exist in the kanaldrv object:
 *   sost_send1 at obj+0x1f4 (8 bytes)
 *   sost_send2 at obj+0x1fc (18 bytes, extended form)
 *   sost_send3 at obj+0x20e (8 bytes)
 *
 * CONFIRMED: the field at [struct+0x2] is the sequence number. It is read
 *   in the command 0x04 handler and in initkandrv::InitSend which tests
 *   [sost+0x2] != 0 before deciding to transmit.
 * ESTIMATED: other field positions.
 */
struct SOST_SEND {
    uint8_t  state;          /* +0x0  transmit state */
    uint8_t  flags;          /* +0x1  control flags */
    uint16_t sequence;       /* +0x2  CONFIRMED: frame sequence/count number */
    uint32_t last_tx_time;   /* +0x4  INFERRED: timestamp of last transmission */
};


/*
 * SOST_TIME_CORRECT
 *
 * GPS time correction state. Lives at kanaldrv object offset +0x1e4.
 * Passed to AnalizBufPriem and IsTimeCorrectAllow.
 *
 * CONFIRMED: 16 bytes at obj+0x1e4.
 * ESTIMATED: internal field layout from field naming conventions and the
 *   time correction function signatures in qmicro.
 */
struct SOST_TIME_CORRECT {
    int32_t  correction_ms;  /* +0x0  time offset from GPS reference in ms */
    uint8_t  enabled;        /* +0x4  time correction enabled flag */
    uint8_t  valid;          /* +0x5  correction value is valid flag */
    uint16_t pad;
    uint32_t last_sync_ms;   /* +0x8  timestamp of last successful GPS sync */
    uint16_t drift_ppb;      /* +0xc  measured clock drift in parts per billion */
    uint16_t pad2;
};


/*
 * TIME_SERVER_KANAL
 *
 * 6-byte BCD timestamp as embedded in RTOS srez frames at bytes 1 through 6.
 *
 * CONFIRMED: 6 bytes copied by MakeSrezWithFictiveTime into frame offsets 1-6
 *   via a memcpy of sizeof(TIME_SERVER_KANAL) = 6 bytes.
 * CONFIRMED BCD format from GPS time conversion functions ConvLocToSerKan in
 *   both the qcet and Aqalpha/pusclass binaries.
 * INFERRED: field ordering YY MM DD HH MM SS based on conventional BCD time
 *   formats and consistency with annotated frame examples.
 */
struct TIME_SERVER_KANAL {
    uint8_t year;    /* BCD, two digits, e.g. 0x24 for the year 2024 */
    uint8_t month;   /* BCD, 0x01 through 0x12 */
    uint8_t day;     /* BCD, 0x01 through 0x31 */
    uint8_t hour;    /* BCD, 0x00 through 0x23 */
    uint8_t minute;  /* BCD, 0x00 through 0x59 */
    uint8_t second;  /* BCD, 0x00 through 0x59 */
};


/*
 * TIME_SERVER
 *
 * Full GPS time server structure, used as input to GetLocalTime and as
 * the GPS shared memory object mapped from //gpstime by gpstime daemon.
 * usodrv::GetLocalTime(TIME_SERVER*, SYSTEMTIME*, long) converts from
 * this format to the Windows-compatible SYSTEMTIME.
 *
 * ESTIMATED: 32 bytes based on typical GPS time structures and the mapping
 *   size implied by the gpstime shared memory segment. Contents unknown
 *   without disassembly of Get_Local_Time in libsystypes.so.1.
 */
struct TIME_SERVER {
    uint8_t raw[32]; /* contents unknown, mapped from the //gpstime segment */
};


/*
 * SYSTEMTIME
 *
 * Windows-compatible SYSTEMTIME structure used as output from GetLocalTime.
 * Standard layout, 16 bytes total.
 *
 * CONFIRMED: presence in multiple function signatures throughout the firmware.
 *   ConvertTimeToSystem(TIME_SERVER*, SYSTEMTIME*) and
 *   ConvertTimeKanalToSystem(TIME_SERVER_KANAL*, SYSTEMTIME*) in libsystypes.
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


/*
 * RTOS_SREZ_HEADER
 *
 * The 10-byte header present at the start of every srez and address frame.
 * Not a named struct in the original source (no symbol for it appears in nm
 * output), but implied by MakeSrezBuf and MakeSrezWithFictiveTime.
 *
 * CONFIRMED: magic 0x75 at offset 0.
 * CONFIRMED: timestamp 6 bytes at offsets 1 through 6.
 * CONFIRMED: type byte at offset 7 (values 0x82, 0x7f, 0x01, 0x02, 0x09).
 * CONFIRMED: 2 zero bytes at offsets 8 and 9.
 */
struct RTOS_SREZ_HEADER {
    uint8_t             magic;      /* always 0x75, ASCII u */
    TIME_SERVER_KANAL   timestamp;  /* 6 bytes BCD */
    uint8_t             type;       /* frame type discriminator */
    uint16_t            reserved;   /* always 0x0000 */
};


/*
 * SUPPORTED_USO_AND_BUF_TYPES
 *
 * Capability bitmap exchanged during driver initialisation. Used by
 * usodrv::SetSupportTypes and usodrv::GetSupportTypes. The QMICRO method
 * UpdateTypeBuf and UpdateTypeUso update these bitmaps as USO devices
 * report their capabilities.
 *
 * ESTIMATED: two 32-bit bitmasks based on the name semantics and the
 *   common pattern of use as a single struct argument across the codebase.
 */
struct SUPPORTED_USO_AND_BUF_TYPES {
    uint32_t uso_type_mask; /* bitmask of supported USO device types */
    uint32_t buf_type_mask; /* bitmask of supported buffer types */
};


/*
 * SOST_BUFFER
 *
 * Ring buffer state for event and data storage. Used by _AddNregBuf,
 * _AddPerBuf, _DeleteEvent, _DeletePerEvent, and related functions in
 * libsystypes.so.1. Also referenced in the channel driver as the
 * transmit ring buffer at [obj+0x9754].
 *
 * ESTIMATED: layout based on typical ring buffer implementations and the
 *   usage patterns seen in libsystypes function signatures.
 */
struct SOST_BUFFER {
    uint16_t head;      /* ESTIMATED: write index */
    uint16_t tail;      /* ESTIMATED: read index */
    uint16_t count;     /* ESTIMATED: current item count */
    uint16_t capacity;  /* ESTIMATED: maximum item count */
    uint8_t  flags;     /* ESTIMATED: status flags */
    uint8_t  pad[3];
};


/*
 * IDENT
 *
 * Device identification block. Used by FindUsoForIdent in ckpris,
 * CopyIdent in libsystypes.so.1, and throughout the KPRIS and SERPR
 * drivers for device matching.
 *
 * ESTIMATED: layout based on the 4-byte identification block observed
 *   at tag[+0x4] being copied into outbound KPRIS frames.
 */
struct IDENT {
    uint8_t data[4]; /* INFERRED: 4-byte device identification block */
};


/*
 * IDENT_PAR_KANAL
 *
 * Extended identification structure for channel parameters. Used by
 * CopyIdent(IDENT_PAR_KANAL*, IDENT*) in libsystypes.so.1 and by
 * _MakeTu(IDENT_PAR_KANAL*, uint8_t, uint8_t).
 *
 * PENDING: full layout requires disassembly of CopyIdent.
 */
struct IDENT_PAR_KANAL {
    IDENT    ident;        /* base identification block */
    uint16_t nom_uso;      /* ESTIMATED: USO device number */
    uint16_t adr_uso;      /* ESTIMATED: USO device address */
    uint8_t  extra[8];     /* ESTIMATED: additional parameters */
};
