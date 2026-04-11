/*
 * altclass_crypto.c
 *
 * Reconstructed implementations of the DES cipher and FCRC18 algorithm
 * from the altclass protocol driver (progr/qalfat).
 *
 * All implementations are derived entirely from static disassembly analysis.
 * No dynamic execution or live system access was performed.
 *
 * The DES permutation tables used by the qalfat binary reside in the .data
 * section of the binary at the addresses documented in altclass_crypto.h.
 * The tables are not reproduced here because:
 *   1. Standard DES tables are well-known and not novel.
 *   2. Reproducing the exact binary data would constitute copying copyrighted
 *      firmware content in violation of the repository license.
 *
 * The algorithm structure and the FCRC18 construction are fully documented.
 * A complete reimplementation using standard DES tables will produce output
 * matching the qalfat binary for any given plaintext and key.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#include "altclass_crypto.h"
#include "crc16.h"
#include <string.h>
#include <stdint.h>


/*
 * Standard DES permutation tables.
 *
 * These are the standard NIST DES tables, identical to those used in the
 * qalfat binary (confirmed by the algorithm structure and S-box behaviour).
 * They are included here from the public domain DES specification, not
 * extracted from the firmware binary.
 */

/* IP: Initial Permutation, 64-bit, 1-based */
static const uint8_t ip_table[64] = {
    58, 50, 42, 34, 26, 18, 10, 2,
    60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6,
    64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17,  9, 1,
    59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5,
    63, 55, 47, 39, 31, 23, 15, 7
};

/* FP: Final Permutation (inverse of IP), 64-bit, 1-based */
static const uint8_t fp_table[64] = {
    40, 8, 48, 16, 56, 24, 64, 32,
    39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30,
    37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28,
    35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26,
    33, 1, 41,  9, 49, 17, 57, 25
};

/* PC1: Permuted Choice 1 (key compression), 56-bit, 1-based */
static const uint8_t pc1_table[56] = {
    57, 49, 41, 33, 25, 17,  9,
     1, 58, 50, 42, 34, 26, 18,
    10,  2, 59, 51, 43, 35, 27,
    19, 11,  3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15,
     7, 62, 54, 46, 38, 30, 22,
    14,  6, 61, 53, 45, 37, 29,
    21, 13,  5, 28, 20, 12,  4
};

/* PC2: Permuted Choice 2 (subkey selection), 48-bit, 1-based */
static const uint8_t pc2_table[48] = {
    14, 17, 11, 24,  1,  5,
     3, 28, 15,  6, 21, 10,
    23, 19, 12,  4, 26,  8,
    16,  7, 27, 20, 13,  2,
    41, 52, 31, 37, 47, 55,
    30, 40, 51, 45, 33, 48,
    44, 49, 39, 56, 34, 53,
    46, 42, 50, 36, 29, 32
};

/* E: Expansion function, 48-bit, 1-based */
static const uint8_t e_table[48] = {
    32,  1,  2,  3,  4,  5,
     4,  5,  6,  7,  8,  9,
     8,  9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17,
    16, 17, 18, 19, 20, 21,
    20, 21, 22, 23, 24, 25,
    24, 25, 26, 27, 28, 29,
    28, 29, 30, 31, 32,  1
};

/* P: Permutation after S-boxes, 32-bit, 1-based */
static const uint8_t p_table[32] = {
    16,  7, 20, 21, 29, 12, 28, 17,
     1, 15, 23, 26,  5, 18, 31, 10,
     2,  8, 24, 14, 32, 27,  3,  9,
    19, 13, 30,  6, 22, 11,  4, 25
};

/* Standard DES S-boxes, 8 boxes of 64 entries each */
static const uint8_t sbox_tables[8][64] = {
    /* S1 */
    {14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7,
      0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8,
      4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0,
     15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
    /* S2 */
    {15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10,
      3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5,
      0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15,
     13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
    /* S3 */
    {10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8,
     13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1,
     13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7,
      1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
    /* S4 */
    { 7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15,
     13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9,
     10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4,
      3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
    /* S5 */
    { 2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9,
     14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6,
      4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14,
     11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
    /* S6 */
    {12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11,
     10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8,
      9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6,
      4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
    /* S7 */
    { 4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1,
     13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6,
      1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2,
      6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
    /* S8 */
    {13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7,
      1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2,
      7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8,
      2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}
};

/* Key schedule rotation amounts per round (1-indexed):
 * rounds 1,2,9,16 rotate by 1 bit; all others rotate by 2 bits.
 * Confirmed from des() disassembly: cmp esi, 0x1 / jbe, cmp ebx, 0x9,
 * cmp ebx, 0x10 checks determine single vs double rotation. */
static const int key_shifts[16] = {1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1};


/*
 * permutation
 *
 * General bit permutation. See altclass_crypto.h for full description.
 */
void
permutation(uint8_t *output, uint8_t *input, uint8_t count,
            const uint8_t *table)
{
    uint8_t temp[64]; /* maximum DES bit array size */
    uint8_t i;

    if (input == NULL) {
        /* in-place: copy output to temp first */
        for (i = 0; i < count; i++)
            temp[i] = output[i];
        input = temp;
    }

    for (i = 0; i < count; i++) {
        /* table is 1-based: subtract 1 for 0-based array indexing */
        output[i] = input[table[i] - 1];
    }
}


/*
 * sboxes
 *
 * Apply one DES S-box lookup. See altclass_crypto.h for full description.
 */
void
sboxes(uint8_t *output, const uint8_t *sbox_input, const uint8_t *sbox_table)
{
    uint8_t index;
    uint8_t result;

    /* Assemble the 6-bit index from the input bit array.
     * The bit ordering confirmed from disassembly at 0x8049dec through
     * 0x8049e1b: bit positions 0,4,3,2,1,5 map to index bits 5,4,3,2,1,0. */
    index = (uint8_t)(
        (sbox_input[0] << 5) |
         sbox_input[4]        |
        (sbox_input[3] << 1)  |
        (sbox_input[2] << 2)  |
        (sbox_input[1] << 3)  |
        (sbox_input[5] << 4)
    );

    result = sbox_table[index];

    /* Unpack 4-bit result into output bit array, MSB first.
     * Confirmed from disassembly at 0x8049e25 through 0x8049e48. */
    output[0] = (result >> 3) & 1;  /* MSB */
    output[1] = (result >> 2) & 1;
    output[2] = (result >> 1) & 1;
    output[3] =  result       & 1;  /* LSB */
}


/*
 * xor_bytes
 *
 * Byte-array XOR. See altclass_crypto.h for full description.
 */
void
xor_bytes(uint8_t *target, const uint8_t *source, uint8_t count)
{
    uint8_t i;
    for (i = 0; i < count; i++) {
        target[i] ^= source[i];
    }
}


/*
 * bytes_to_bit
 *
 * Expand 8 bytes to 64 per-bit elements. See altclass_crypto.h.
 */
void
bytes_to_bit(const uint8_t *bytes_in, uint8_t *bits_out)
{
    int i;
    for (i = 0; i < 64; i++) {
        int byte_idx = i / 8;
        int bit_pos = 7 - (i % 8);  /* MSB first within each byte */
        bits_out[i] = (bytes_in[byte_idx] >> bit_pos) & 1;
    }
}


/*
 * bits_to_bytes
 *
 * Collapse 64 per-bit elements to 8 bytes. See altclass_crypto.h.
 */
void
bits_to_bytes(const uint8_t *bits_in, uint8_t *bytes_out)
{
    int i;
    memset(bytes_out, 0, 8);
    for (i = 0; i < 64; i++) {
        if (bits_in[i]) {
            int byte_idx = i / 8;
            int bit_pos = 7 - (i % 8);
            bytes_out[byte_idx] |= (uint8_t)(1 << bit_pos);
        }
    }
}


/*
 * des_bits
 *
 * Core DES cipher on bit arrays. See altclass_crypto.h for full description.
 * This implements the 16-round Feistel structure confirmed from disassembly.
 */
void
des_bits(uint8_t *block_bits, uint8_t *key_bits, int mode)
{
    uint8_t permuted[64];
    uint8_t key_56[56];
    uint8_t c[28], d[28];
    uint8_t subkeys[16][48];
    uint8_t left[32], right[32];
    uint8_t expanded[48];
    uint8_t xored[48];
    uint8_t sbox_out[32];
    uint8_t perm_out[32];
    uint8_t temp[32];
    int round, i, box;

    /* Initial permutation */
    permutation(permuted, block_bits, 64, ip_table);
    memcpy(block_bits, permuted, 64);

    /* Key schedule: PC1 */
    permutation(key_56, key_bits, 56, pc1_table);

    /* Split key into C and D halves */
    memcpy(c, key_56,      28);
    memcpy(d, key_56 + 28, 28);

    /* Generate 16 subkeys */
    for (round = 0; round < 16; round++) {
        uint8_t subkey_expanded[56];
        int shift = key_shifts[round];
        uint8_t c_temp, d_temp;
        int s;

        /* Rotate C and D by the round shift amount */
        for (s = 0; s < shift; s++) {
            c_temp = c[0];
            d_temp = d[0];
            memmove(c, c + 1, 27);
            memmove(d, d + 1, 27);
            c[27] = c_temp;
            d[27] = d_temp;
        }

        /* Concatenate C and D for PC2 */
        memcpy(subkey_expanded,      c, 28);
        memcpy(subkey_expanded + 28, d, 28);

        /* PC2: select 48 bits for the subkey */
        permutation(subkeys[round], subkey_expanded, 48, pc2_table);
    }

    /* Split block into L and R */
    memcpy(left,  block_bits,      32);
    memcpy(right, block_bits + 32, 32);

    /* 16 Feistel rounds */
    for (round = 0; round < 16; round++) {
        int key_round = (mode == DES_MODE_ENCRYPT) ? round : (15 - round);

        /* Expand R from 32 to 48 bits */
        permutation(expanded, right, 48, e_table);

        /* XOR with round key */
        memcpy(xored, expanded, 48);
        xor_bytes(xored, subkeys[key_round], 48);

        /* S-box substitution: 8 S-boxes, 6 bits each in, 4 bits each out */
        for (box = 0; box < 8; box++) {
            sboxes(sbox_out + box * 4, xored + box * 6,
                   sbox_tables[box]);
        }

        /* P permutation */
        permutation(perm_out, sbox_out, 32, p_table);

        /* XOR with L */
        memcpy(temp, right, 32);  /* save R as new L */
        memcpy(right, left, 32);
        xor_bytes(right, perm_out, 32);  /* new R = old L XOR f(old R, key) */
        memcpy(left, temp, 32);   /* new L = old R */
    }

    /* Combine: note R comes before L after the last round (standard DES) */
    memcpy(block_bits,      right, 32);
    memcpy(block_bits + 32, left,  32);

    /* Final permutation */
    permutation(permuted, block_bits, 64, fp_table);
    memcpy(block_bits, permuted, 64);
}


/*
 * encrypt_block
 *
 * In-place DES encryption. See altclass_crypto.h for full description.
 */
void
encrypt_block(uint8_t *plaintext, const uint8_t *key)
{
    uint8_t plain_bits[64];
    uint8_t key_bits[64];

    bytes_to_bit(plaintext, plain_bits);
    bytes_to_bit(key,       key_bits);

    des_bits(plain_bits, key_bits, DES_MODE_ENCRYPT);

    bits_to_bytes(plain_bits, plaintext);
}


/*
 * fend_send_append_crc
 *
 * Appends FCRC18 to a frame buffer. See altclass_crypto.h for description.
 * Implementation confirmed from fend_send disassembly at 0x804c5b3.
 */
uint16_t
fend_send_append_crc(uint8_t *buf, uint16_t len)
{
    uint16_t crc;

    /* Select CRC coverage range based on frame type byte at buf[0].
     * Confirmed from the cmp at 0x804c5c1: if buf[0] == 0x6, use sub-frame. */
    if (buf[0] == 0x06) {
        /* Sub-frame: CRC covers buf[1..len-1] (skip the type byte) */
        crc = fcrc18(buf + 1, (uint16_t)(len - 1));
    } else {
        /* Full frame: CRC covers buf[0..len-1] */
        crc = fcrc18(buf, len);
    }

    /* Append CRC high byte then low byte, confirmed from disassembly:
     * shr ax, 0x8 -> high byte, mov cl, low byte */
    buf[len]     = (uint8_t)((crc >> 8) & 0xFF);
    buf[len + 1] = (uint8_t)(crc & 0xFF);

    return (uint16_t)(len + 2);
}
