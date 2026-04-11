/*
 * cusom_types.h
 *
 * Type definitions specific to the cusom USO field bus driver family
 * (progr/usom and progr/usom2).
 *
 * The cusom class implements a third distinct USO field bus class, separate
 * from both cusotm (USOTM) and the RTU class polling drivers (altclass,
 * aclass, etc.). Its tag struct layout differs from both of those families.
 *
 * Sources: disassembly of cusom::SendZaprosSerNom, cusom::ZaprosSetTi,
 *   cusom::ZaprosTestTs, cusom::RaspakTestDiscret, cusom::SetGroupTu in
 *   progr/usom2. nm output for both usom and usom2 binaries.
 *
 * Confidence markers:
 *   CONFIRMED   offset verified directly from disassembly byte accesses
 *   INFERRED    derived from usage context
 *   ESTIMATED   size is reasonable based on surrounding confirmed offsets
 *   PENDING     requires further disassembly
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>


/*
 * CUSOM_TAG
 *
 * The device tag struct for the cusom USO field bus family. Accessed via
 * cusom::GetUsomTag(iocuso*), which returns a pointer to the tag struct
 * associated with a given iocuso device handle.
 *
 * This is a different struct from the USA struct used by the RTU class
 * polling drivers (altclass, aclass, etc.). The cusom family uses GetUsomTag
 * while the RTU class uses GetUsomUSA.
 *
 * The iocuso device array in the cusom object starts at object offset +0x1c0.
 * Individual iocuso pointers are stored as [array + slot_index * 4].
 */
struct CUSOM_TAG {
    uint8_t  device_addr;    /* +0x00  CONFIRMED: tag[0] used as device address
                                in SendZaprosSerNom, ZaprosSetTi, ZaprosTestTs,
                                ZaprosTs, and SetGroupTu. The first byte of
                                the tag struct is always the device address */

    uint8_t  pad_01[0x1];    /* +0x01  gap byte */

    uint8_t  active_flag;    /* +0x01? INFERRED: tag[+0x1] compared against 0x1
                                in the Working loop test at [eax+0x1] before
                                ZaprosSetTi is called. Non-zero means the
                                device is actively configured for this slot */

    uint8_t  pad_02[0x25];   /* +0x02 through +0x27  unknown fields */

    /* Object offset 0x28 relative to the iocuso pointer:
     * [iocuso+0x28] compared against 0 with jle (signed comparison) in the
     * Working loop before ZaprosSetTi is called. This is a TI point count
     * or pending TI value count. */
    uint32_t ti_count;       /* +0x28  CONFIRMED: [eax+0x28] > 0 required for
                                ZaprosSetTi to be called */

    uint8_t  pad_2c[0x27c - 0x2c]; /* gap to the SetGroupTu fields */

    uint16_t group_tu_word_b;  /* +0x27c  CONFIRMED: movzx eax, WORD PTR
                                   [eax+0x27c] in SetGroupTu (usom2), low byte
                                   stored in outbound frame */

    uint8_t  group_tu_byte_a;  /* +0x27d  CONFIRMED: movzx eax, BYTE PTR
                                   [eax+0x27d] in SetGroupTu */

    uint16_t group_tu_word_a;  /* +0x27e  CONFIRMED: movzx eax, WORD PTR
                                   [eax+0x27e] in SetGroupTu */

    uint8_t  group_tu_mask;    /* +0x27f  CONFIRMED: movzx eax, BYTE PTR
                                   [eax+0x27f] in SetGroupTu, the group
                                   telecontrol bitmask */

    uint8_t  pad_280[6];       /* +0x280 through +0x285  unknown fields */

    uint8_t  ti_pending[8];    /* +0x286  CONFIRMED: BYTE PTR [eax+ebx*1+0x286]
                                   in ZaprosSetTi, indexed by slot 0-7.
                                   Non-zero indicates a pending TI write
                                   for that slot */

    uint8_t  pad_28e_minus_1;  /* +0x28d  gap byte */

    uint16_t ti_values[8];     /* +0x28e  CONFIRMED: WORD PTR [eax+ebx*2+0x28e]
                                   in ZaprosSetTi, the pending TI write values
                                   for slots 0-7 */
};


/*
 * CUSOM_TEST_RESULT
 *
 * Fields written by RaspakTestDiscret in usom2 after a successful
 * ZaprosTestTs/RaspakTestDiscret cycle.
 *
 * The test results are stored in the CUSOM_TAG at these offsets:
 */
#define CUSOM_TAG_OFFSET_TEST_ACTUAL    0x204u  /* CONFIRMED: [ecx+ebx+0x204]
                                                    actual test value bytes,
                                                    indexed by pair number */
#define CUSOM_TAG_OFFSET_TEST_STATUS    0x20cu  /* CONFIRMED: [ecx+ebx+0x20c]
                                                    test status bytes,
                                                    indexed by pair number */


/*
 * CUSOM frame command type bytes (byte at frame offset 1 in outbound frames).
 * The start byte at offset 0 is always 0x5b, same as USOTM.
 */
#define CUSOM_CMD_SERIAL_NOM  0x5fu  /* CONFIRMED: SendZaprosSerNom frame[1] */
#define CUSOM_CMD_SET_TI      0x6bu  /* CONFIRMED: ZaprosSetTi frame[1] */
#define CUSOM_CMD_TEST_TS     0x63u  /* CONFIRMED: ZaprosTestTs frame[1],
                                        also the echo type checked in
                                        RaspakTestDiscret buf[1] */
#define CUSOM_CMD_SET_GROUP_TU 0x4cu /* CONFIRMED: SetGroupTu frame[1] */

/*
 * The following type bytes are shared between USOTM and USOM, suggesting
 * protocol heritage or compatibility:
 */
#define CUSOM_TYPE_DISCRET    0x57u  /* confirmed: ZaprosTs -> RaspakDiscret */
#define CUSOM_TYPE_IMPULS     0x56u  /* confirmed: ZaprosImpuls -> RaspakImpuls */

/*
 * Frame lengths for usom2 commands, confirmed from disassembly:
 */
#define CUSOM_LEN_SERIAL_NOM  4u   /* SendZaprosSerNom: 0x5b 0x5f addr 0x00 */
#define CUSOM_LEN_SET_TI      21u  /* ZaprosSetTi: header(5) + 8 slots * 2 bytes */
#define CUSOM_LEN_TEST_TS     5u   /* ZaprosTestTs: 0x5b 0x63 addr 0x01 0x01 */


/*
 * Object layout constants for the cusom object.
 * These are offsets into the cusom (this) pointer.
 */
#define CUSOM_OBJ_IOCUSO_ARRAY  0x1c0u  /* CONFIRMED: [esi+0x1c0] loads the
                                            iocuso pointer array. Individual
                                            entries at [array + slot * 4] */

#define CUSOM_OBJ_RESET_COUNTER 0x514cu /* CONFIRMED: WORD PTR [esi+0x514c]
                                            the error saturation counter in usom2
                                            reset to 0 when it exceeds NumUso()*2
                                            or 0x14, triggering port reset */

#define CUSOM_OBJ_FD_SERIAL     0x4e18u /* INFERRED from USOTM pattern:
                                            the serial port file descriptor
                                            stored at this offset in the object.
                                            PENDING: not directly confirmed from
                                            cusom disassembly */

/*
 * Timeout parameters for cusom WaitOtvet calls, expressed in iterations
 * (each iteration is approximately 10ms):
 */
#define CUSOM_TIMEOUT_STANDARD  0x100u  /* 256 iterations, 2.56 seconds.
                                           Used by SendZaprosSerNom, ZaprosSetTi,
                                           ZaprosTestTs, and ZaprosTs */


/*
 * usom vs usom2 signature differences.
 *
 * These function signatures differ between the two binaries and make them
 * incompatible at the call site level:
 *
 * cusom::AddDoutInQuery:
 *   usom:  (unsigned char, unsigned short, unsigned char, unsigned short)
 *   usom2: (unsigned short, unsigned char, unsigned short, unsigned char,
 *            unsigned short)
 *   difference: usom2 adds a leading unsigned short (extended address word)
 *
 * cusom::SendTuCommand:
 *   usom:  (unsigned char, unsigned short, unsigned char, unsigned short)
 *   usom2: (unsigned short, unsigned char, unsigned short, unsigned char,
 *            unsigned short)
 *   same change as AddDoutInQuery (consistent, since AddDoutInQuery feeds
 *   SendTuCommand)
 */


/*
 * Global variables unique to usom2, confirmed from nm:
 */

/* FL_Test at 0x0804df50: enables test mode (ZaprosTestTs in Working loop).
 * When non-zero, both ZaprosSetTi and ZaprosTestTs are active in the
 * Working cycle. When zero, both are skipped. */
#define CUSOM2_FLTTEST_ADDR   0x0804df50u

/* Pause at 0x0804df4c: inter-poll pause in milliseconds, parsed from the
 * Uso= command line by RaspakKeys at startup. Absent from usom. */
#define CUSOM2_PAUSE_ADDR     0x0804df4cu
