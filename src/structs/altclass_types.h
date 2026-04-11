/*
 * altclass_types.h
 *
 * Type definitions for the altclass protocol driver (progr/qalfat) and
 * the aclass supervisor (progr/Aqalpha), and the shared USA struct used
 * by all RTU class polling drivers (qcet, qmir, qpuso, qpty, qptym).
 *
 * The USA struct is the central device state block for the entire RTU class
 * polling driver family. It is accessed via iocuso+0x20 using the single-
 * dereference GetUsomUSA pattern confirmed in altclass, aclass, pusclass,
 * mirclass, ctclass, pclass, and ptmclass.
 *
 * Sources: disassembly of altclass::GetDiscret, altclass::GetAnalog,
 *   altclass::KorAdr, ftek18, fopimpt, fend_send, Read_OK181 in progr/qalfat.
 *   nm output for all RTU class polling binaries.
 *   aclass method table from nm of progr/Aqalpha.
 *
 * Confidence markers:
 *   CONFIRMED   offset verified directly from disassembly byte accesses
 *   INFERRED    derived from usage context and call site analysis
 *   ESTIMATED   size is known, layout is a reasonable guess
 *   PENDING     requires further disassembly
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>


/*
 * USA
 *
 * The central device state struct for the RTU class polling driver family.
 * Every driver in this family (altclass, aclass, pusclass, mirclass, ctclass,
 * pclass, ptmclass) accesses this struct via the GetUsomUSA single-dereference
 * at iocuso+0x20.
 *
 * The name USA likely stands for a Russian abbreviation for device state
 * block or similar (Устройство Связи с Абонентом, Unified Status Area, or
 * similar). The name is not recoverable from static analysis alone.
 *
 * The struct is large (estimated 0x124+ bytes based on the highest confirmed
 * offset +0x119 plus the float coefficients at +0x11c and +0x120). The config
 * subblock starting at +0x124 is accessed as a separate region (AconfK offset
 * in altclass, computed as AdrUso + 0x124).
 *
 * All offset confirmations are from altclass (qalfat) disassembly unless
 * otherwise noted.
 */
struct USA {
    uint8_t  device_addr;         /* +0x00  CONFIRMED: [ebx+0x0] in KorAdr,
                                     the device address byte for frame
                                     construction */

    uint8_t  pad_01[0x1f];        /* +0x01 through +0x1f  unknown fields */

    /* +0x20: this offset is where the USA pointer itself is stored in iocuso.
     * The struct member at +0x20 would be iocuso's internal field, not a
     * field within USA. The USA struct begins at the address pointed to by
     * iocuso[+0x20]. */

    uint8_t  pad_20[0x0a];        /* +0x20 through +0x29  unknown fields */

    uint16_t current_ui;          /* +0x2a  CONFIRMED: movzx eax, WORD PTR
                                     [eax+0x2a] in GetAnalog, compared against
                                     TekUI at 0x8052a60 for UI reference check.
                                     Likely a current measurement or step
                                     index for the current operating point */

    uint8_t  pad_2c_minus_1;      /* +0x2c  boundary byte */

    uint8_t  mode_type;           /* +0x2c  CONFIRMED: [edx+0x2c] compared
                                     against 1 and 2, a mode discriminator or
                                     device type byte selecting between two
                                     operating modes */

    uint8_t  pad_2d[1];           /* +0x2d  boundary, device_key starts here */

    uint8_t  device_key[8];       /* +0x2d through +0x34  CONFIRMED: [eax+edx+0x2d]
                                     in fopimpt encryption sequence, the 8-byte
                                     device-specific key block used as one of
                                     the two DES inputs */

    uint8_t  pad_35[1];           /* +0x35  gap byte */

    uint16_t channel_bitmask;     /* +0x36  INFERRED: analog channel enable
                                     bitmask, 16 channels */

    uint8_t  pad_38[1];           /* +0x38  gap byte */

    uint8_t  comms_ok_flag;       /* +0x38  CONFIRMED: [eax+0x38] compared
                                     against 0 in the state check at
                                     0x804cacd, non-zero means the device
                                     has completed its initialisation handshake
                                     and is ready for data polling */

    uint8_t  device_address;      /* +0x39  CONFIRMED: [eax+0x39] compared
                                     against 0 in the device-configured check
                                     in GetDiscret and in the fopimpt state
                                     machine at 0x804cabd. Zero means no
                                     device is configured for this slot */

    uint8_t  pad_3a[0x3e];        /* +0x3a through +0x77  unknown fields
                                     likely include device model code, serial
                                     number, firmware version string, and
                                     other identification fields */

    uint32_t time_components[11]; /* +0x78 through +0xa0  CONFIRMED: eleven
                                     consecutive uint32 fields starting at
                                     [eax+0x78], confirmed as localtime()
                                     struct fields (tm_sec, tm_min, tm_hour,
                                     tm_mday, tm_mon, tm_year, tm_wday,
                                     tm_yday, tm_isdst, plus two additional
                                     fields for GPS-corrected time) */

    uint8_t  pad_a4[0x5a];        /* +0xa4 through +0xfd  unknown fields
                                     likely include measurement values,
                                     error counters, and status registers */

    uint8_t  busy_lock;           /* +0xfe  CONFIRMED: [edx+0xfe] compared
                                     against 0 in GetDiscret. Non-zero means
                                     the device is busy (in active
                                     communication with the fopimpt thread)
                                     and the value delivery path must wait */

    uint16_t retry_counter;       /* +0x100  CONFIRMED: [eax+0x100] incremented
                                     in fopimpt when Read_OK181 returns 0x6
                                     or 0x1. Reset to 0 when retry threshold
                                     is exceeded. WORD PTR confirmed */

    uint8_t  state_byte;          /* +0x102  CONFIRMED: [eax+0x102] set to 0x0
                                     (normal), 0x1 (transmit error), 0x4 (error
                                     state), and 0x32 (special preserved state)
                                     in fopimpt. Also set to 0x0 in KorAdr
                                     when the device address changes */

    uint8_t  saturation_cnt;      /* +0x103  CONFIRMED: [eax+0x103] capped at
                                     0x14 (20) in GetDiscret, used as the
                                     threshold counter for the discrete state
                                     threshold detection. Values 0-9 map to
                                     type 0x5a output (open), values 10-20
                                     map to type 0x5b output (closed) */

    uint8_t  slot_flag;           /* +0x104  CONFIRMED: [ebx+eax+0x104] in
                                     KorAdr, cleared when the current address
                                     differs from the address parameter value.
                                     Part of the address correction tracking */

    uint8_t  pad_105[0x0f];       /* +0x105 through +0x113  unknown fields */

    uint16_t config_word_a;       /* +0x114  INFERRED: configuration word A */

    uint16_t config_word_b;       /* +0x116  INFERRED: configuration word B */

    uint8_t  pad_118[1];          /* +0x118  gap byte */

    uint8_t  unit_conv_flag;      /* +0x119  CONFIRMED: [eax+0x119] in ftek18,
                                     selects between two multiplier constants:
                                     0 = primary units (multiplier at 0x8050f88)
                                     non-zero = secondary units (0x8050f90)
                                     used for metric/imperial or primary/
                                     secondary winding ratio selection */

    uint8_t  pad_11a[2];          /* +0x11a through +0x11b  gap bytes */

    float    coeff_primary;       /* +0x11c  CONFIRMED: fld DWORD PTR [eax+0x11c]
                                     in GetAnalog, the primary scaling
                                     coefficient for analog value conversion
                                     from raw ADC counts to engineering units */

    float    coeff_secondary;     /* +0x120  CONFIRMED: fld DWORD PTR [eax+0x120]
                                     in GetAnalog, the secondary scaling
                                     coefficient selected when unit_conv_flag
                                     is non-zero */

    /* +0x124: start of config subblock (AconfK in altclass, accessed as
     * AdrUso + 0x124 = Aconf at 0x8054640). The config subblock likely
     * contains device-specific configuration parameters used by the
     * fopimpt session handshake. ESTIMATED size: 0x100 bytes minimum. */
    uint8_t  config_subblock[256]; /* +0x124  ESTIMATED: configuration data
                                      for the session handshake, read by the
                                      fopimpt state machine during the device
                                      type validation phase */
};


/*
 * MADR_TABLE
 *
 * The device slot pointer table. Stored at global address 0x80547a0 in
 * the altclass binary. Each entry is a pointer to a USA struct for one
 * device slot.
 *
 * The device count is stored at 0x8052e08 (KolUsrv confirmed from the
 * nm output showing it in the data segment).
 *
 * Maximum observed usage: at least 5 slots (confirmed from the ftek18
 * loop bound cmp ecx, 0x5 and the AddUserDataAnalog 12-entry table).
 */
#define MADR_MAX_SLOTS  16  /* ESTIMATED: upper bound on device slot count */

typedef USA *MADR_TABLE[MADR_MAX_SLOTS];


/*
 * ALTCLASS_SESSION_STATE
 *
 * The session state variables maintained by fopimpt in the altclass binary.
 * These are not a contiguous struct in the original code; they are individual
 * global variables in the data segment. This grouping is for documentation
 * purposes only.
 *
 * All addresses are from the altclass (qalfat) binary.
 */
struct ALTCLASS_SESSION_STATE {
    /* 0x8052ea0  SOST_KAN: channel session state byte, checked at loop top */
    /* 0x8052ea1  FL_R: device slot read index */
    /* 0x8052ea2  SHP: session high protocol byte, the state machine state */
    /* 0x8052ea3  KolUsrv: number of active service connections */
    /* 0x8052ea4  KolSend: outbound frame length counter */
    /* 0x8052ea6  KolPriem: receive position counter */
    /* 0x8052eaa  NPT: no-protocol flag */
    /* 0x8052eac  KolPr: receive base counter */
    /* 0x8052eae  sh: result from most recent Read_OK181 call */
    /* 0x8052ec0  BufSend[320]: outbound frame buffer */
    /* 0x8053000  BufPriem[variable]: inbound frame buffer */
    /* 0x8054580  Flag: port acquisition flag */
    /* 0x8054582  BZ: buffer zone index */
    /* 0x805457c  pbuf: pointer into current receive position */
    /* 0x8054784  PUSRV: USA struct pointer for primary device */
    /* 0x8054788  AdrUso: current active device USA pointer */
    /* 0x805478c  PRV: cached GetUsomUSA result */
};


/*
 * ALTCLASS_CRYPTO_KEY
 *
 * The two 8-byte key blocks used in the altclass DES encryption sequence.
 * These are global buffers in the data segment, not a struct in the original.
 *
 * mk2 at 0x8052a00: the device-specific key, read from USA[+0x2d] to USA[+0x34]
 * mk1 at 0x8052a09: the session key, read from the received frame at
 *   pbuf[KolPr+1] through pbuf[KolPr+8]
 *
 * The DES operation is: encrypt(mk2, mk1), meaning the device key is the
 * plaintext and the session key is used as the DES key. The result stored
 * back into mk2 is the authenticated response.
 */
struct ALTCLASS_CRYPTO_KEY {
    uint8_t device_key[8]; /* mk2 at 0x8052a00, from USA[+0x2d..+0x34] */
    uint8_t session_key[8]; /* mk1 at 0x8052a09, from received frame */
};


/*
 * FCRC18_PARAMS
 *
 * Documents the FCRC18 algorithm parameters for reference. Not an actual
 * struct in the original code.
 *
 * The FCRC18 algorithm (see src/protocol/altclass_crypto.c):
 *   seed = ((~buf[1] & 0xff) << 8) | (~buf[0] & 0xff)
 *   for i in range(2, len):
 *     seed = crc16_kermit(buf[i], seed)
 *   seed = crc16_kermit(0, seed)
 *   seed = crc16_kermit(0, seed)
 *   result = ~seed & 0xffff
 *   result = ((result << 8) | (result >> 8)) & 0xffff
 *   return result
 *
 * Where crc16_kermit processes one byte with polynomial 0x8408 (reflected
 * 0x1021, CRC-16/KERMIT variant).
 */


/*
 * FTEK18_SCALE_SLOT
 *
 * One entry in the ftek18 measurement scaling table. Five slots exist
 * (loop bound cmp ecx, 0x5) with stride 0xa8 bytes each.
 *
 * ESTIMATED: layout based on the ftek18 disassembly showing a 17-byte
 *   clear at the start of each slot and a float output at [eax*4+0x8052a64].
 */
struct FTEK18_SCALE_SLOT {
    uint8_t  clear_region[17]; /* ESTIMATED: cleared to 0 at start of each
                                   poll cycle, likely includes validity flags
                                   and intermediate calculation fields */
    uint8_t  pad[0xa8 - 17];  /* ESTIMATED: remaining fields in the stride */
};


/*
 * Addresses of the scaling output array and the two multiplier constants
 * confirmed from ftek18 disassembly.
 *
 * The scaling output array starts at 0x8052a64 and contains float values
 * indexed by channel number: output[channel] = raw_value * multiplier.
 */
#define FTEK18_OUTPUT_ARRAY_ADDR  0x8052a64u
#define FTEK18_MULTIPLIER_PRIMARY  0x8050f88u  /* primary units multiplier */
#define FTEK18_MULTIPLIER_SECONDARY 0x8050f90u /* secondary units multiplier */


/*
 * Device type code range validated by ftek18 and altclass drivers.
 *
 * Valid device type bytes (TypeUso values) for the altclass protocol:
 *   0x14 (20) through 0x31 (49) inclusive
 *   confirmed from: lea eax,[edx-0x14] / cmp eax, 0x1d in AddUserDataAnalog
 *   confirmed from: lea eax,[edx-0x15] / cmp ax, 0x1c in ftek18
 *
 * The slight difference (0x14 vs 0x15) between the two checks suggests
 * AddUserDataAnalog accepts one more type code at the bottom of the range
 * than ftek18 expects, likely because type 0x14 is a measurement point
 * that does not require the ftek18 scaling path.
 */
#define ALTCLASS_TYPE_MIN_ANALOG  0x14u  /* minimum valid analog type byte */
#define ALTCLASS_TYPE_MAX_ANALOG  0x31u  /* maximum valid analog type byte */
#define ALTCLASS_TYPE_MIN_FTEK18  0x15u  /* minimum type for ftek18 scaling */

/*
 * Valid discrete type codes in altclass::GetDiscret:
 *   0x5a: "closed" relay contact state
 *   0x5b: "open" relay contact state
 *   check: lea eax,[ecx-0x5a] / cmp ax, 0x1 (accepts only 0x5a and 0x5b)
 */
#define ALTCLASS_TYPE_DISCRET_CLOSED  0x5au
#define ALTCLASS_TYPE_DISCRET_OPEN    0x5bu

/*
 * Saturation counter threshold for discrete state output:
 *   values 0 through 9 map to type 0x5a (closed/inactive)
 *   values 10 through 20 map to type 0x5b (open/active)
 *   cap: saturation_cnt is clamped to 0x14 (20) before the threshold check
 */
#define ALTCLASS_SAT_THRESHOLD  0x0au   /* 10: threshold between states */
#define ALTCLASS_SAT_MAX        0x14u   /* 20: maximum saturation counter */
