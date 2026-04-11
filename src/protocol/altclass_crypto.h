/*
 * altclass_crypto.h
 *
 * Interface for the DES cipher and FCRC18 algorithm reconstructed from
 * the altclass protocol driver (progr/qalfat) and the aclass supervisor
 * (progr/Aqalpha).
 *
 * The DES implementation in the qalfat binary is a standard 16-round
 * Feistel network with IP, FP, PC1, PC2, and E permutations, 8 S-boxes,
 * and a complete key schedule. The encrypt() function operates on 64-bit
 * (8-byte) blocks. All internal operations use per-bit arrays (expanded
 * via bytestobit) rather than byte arrays directly.
 *
 * The FCRC18 algorithm is a proprietary three-layer construction built on
 * CRC-16/KERMIT (polynomial 0x8408). It is not a standard CRC variant.
 * Full algorithm description is in crc16-reconstructed.c.
 *
 * The Aqalpha binary adds encrypt17(char*, char*, char*) which takes three
 * pointers rather than two. This may be a 17-round DES variant, a 17-block
 * cipher, or a different algorithm entirely. Disassembly of that function
 * is required for clarification.
 *
 * Authorization: view-only research license. See LICENSE.
 */

#pragma once
#include <stdint.h>
#include <stddef.h>


/*
 * DES block size in bytes.
 * All cipher operations work on 8-byte (64-bit) blocks.
 */
#define DES_BLOCK_SIZE  8

/*
 * DES key size in bytes.
 * The key input to encrypt() and des() is 8 bytes (64 bits).
 * 8 bits are used for parity (standard DES), leaving 56 effective key bits.
 */
#define DES_KEY_SIZE    8

/*
 * DES per-bit array size.
 * bytestobit and bitstobytes operate on 64-element bit arrays (one element
 * per bit of the 64-bit block), confirmed from cmp esi, 0x40 in bytestobit
 * at 0x8049e90 and cmp edi, 0x40 in bitstobytes at 0x804c7bf.
 */
#define DES_BIT_ARRAY_SIZE  64


/*
 * DES operation modes.
 * Confirmed from des() disassembly: mode parameter at [ebp+0x10].
 * Mode 0 (decrypt) reverses the key schedule order.
 * Mode 1 (encrypt) uses the forward key schedule order.
 */
#define DES_MODE_DECRYPT  0
#define DES_MODE_ENCRYPT  1


/*
 * des
 *
 * Core DES cipher function.
 * Signature: des(unsigned char* plaintext_bits, unsigned char* key_bits,
 *               int mode)
 *
 * Both input pointers are per-bit arrays (64 elements each), not byte arrays.
 * The caller must first call bytestobit() to expand byte arrays to bit arrays.
 * The result is written back into the plaintext_bits array in place.
 *
 * Confirmed from disassembly at 0x804b196 in qalfat:
 *   First action: Permutation(output, plaintext_bits, 0x38, perm1)
 *     applies IP (initial permutation) from table at 0x8052460, 56 bits
 *   Key schedule: calls Permutation with perm2 (PC1 at 0x80524a0), then
 *     iterates 16 rounds, each calling Permutation with the round key
 *     from keys[round * 0x30] (48 bits per round key)
 *   Round scheduling: rounds 1, 9, and 16 (indices 0, 8, and 15) use
 *     single left rotation (esi <= 1 or ebx == 9 or ebx == 16 checks)
 *     all other rounds use double left rotation
 *   Final permutation: Permutation with perm4 (FP at 0x8052520), 64 bits
 *
 * The mode parameter controls key schedule direction:
 *   DES_MODE_DECRYPT (0): key schedule in reverse order for decryption
 *   DES_MODE_ENCRYPT (1): key schedule in forward order for encryption
 */
void des_bits(uint8_t *plaintext_bits, uint8_t *key_bits, int mode);


/*
 * Permutation
 *
 * General bit permutation function.
 * Signature: Permutation(unsigned char* output, unsigned char* input,
 *                        unsigned char count, unsigned char* table)
 *
 * Confirmed from disassembly at 0x804b127 in qalfat.
 *
 * If input is NULL, copies count bits from output into a local stack buffer
 * first, implementing an in-place permutation.
 *
 * Core loop (0x804b17b): for each position i from 0 to count-1:
 *   output[i] = input[table[i] - 1]
 *
 * The -1 converts from the 1-based indexing used in DES specification to
 * 0-based array indexing, confirming the permutation tables are stored in
 * standard DES 1-based form.
 *
 * Parameters:
 *   output: output bit array
 *   input:  input bit array (or NULL for in-place)
 *   count:  number of bits to permute
 *   table:  1-based permutation table of length count
 */
void permutation(uint8_t *output, uint8_t *input, uint8_t count,
                 const uint8_t *table);


/*
 * SBoxes
 *
 * Apply one DES S-box lookup.
 * Signature: SBoxes(unsigned char* output, unsigned char* sbox_input,
 *                   unsigned char* sbox_table)
 *
 * Confirmed from disassembly at 0x8049de2 in qalfat.
 *
 * sbox_input is a 6-element bit array. The function assembles the row and
 * column index from the bits:
 *   edx = input[0] << 5           (outermost bit, row high)
 *       | input[4]                 (outermost bit, row low)
 *       | input[3] << 1            (column bit 0)
 *       | input[2] << 2            (column bit 1)
 *       | input[1] << 3            (column bit 2)
 *       | input[5] << 4            (column bit 3)
 *
 * This assembles: row = (bit0 << 1) | bit5, column = bits 1-4.
 * Standard DES S-box addressing: outer bits select row, inner 4 bits select
 * column.
 *
 * sbox_table[edx] gives a 4-bit value which is unpacked into output[3..0]:
 *   output[3] = result & 1   (LSB)
 *   output[2] = (result >> 1) & 1
 *   output[1] = (result >> 2) & 1
 *   output[0] = (result >> 3) & 1  (MSB)
 *
 * Parameters:
 *   output:      4-element bit array for the 4-bit S-box output
 *   sbox_input:  6-element bit array for the 6-bit S-box input
 *   sbox_table:  64-entry S-box table (one S-box)
 */
void sboxes(uint8_t *output, const uint8_t *sbox_input,
            const uint8_t *sbox_table);


/*
 * Xor
 *
 * Byte-array XOR operation.
 * Signature: Xor(unsigned char* target, unsigned char* source,
 *                unsigned char count)
 *
 * Confirmed from disassembly at 0x8049d66 in qalfat.
 *
 * Performs: target[i] ^= source[i] for i from 0 to count-1.
 * Used in the DES round function to XOR the round key with the expanded
 * half-block.
 */
void xor_bytes(uint8_t *target, const uint8_t *source, uint8_t count);


/*
 * bytestobit
 *
 * Expand a byte array to a per-bit representation.
 * Signature: bytestobit(unsigned char* bytes_in, unsigned char* bits_out)
 *
 * Confirmed from disassembly at 0x8049e4b in qalfat.
 * Iterates 64 positions (cmp esi, 0x40).
 *
 * For each bit position i from 0 to 63:
 *   byte_index = i / 8  (with adjustment for i386 sign extension)
 *   bit_mask = 1 << (7 - (i % 8))
 *   bits_out[i] = (bytes_in[byte_index] & bit_mask) ? 1 : 0
 *
 * Parameters:
 *   bytes_in: 8-byte input array
 *   bits_out: 64-element output array, one uint8 per bit
 */
void bytes_to_bit(const uint8_t *bytes_in, uint8_t *bits_out);


/*
 * bitstobytes
 *
 * Collapse a per-bit array back to byte form.
 * Signature: bitstobytes(unsigned char* bits_in, unsigned char* bytes_out)
 *
 * Confirmed from disassembly at 0x804c768 in qalfat.
 * First zeroes the output with memset(bytes_out, 0, 8).
 * Iterates 64 positions (cmp edi, 0x40).
 *
 * For each bit position i from 0 to 63:
 *   if bits_in[i] is set:
 *     bytes_out[i/8] |= (1 << (7 - (i % 8)))
 *
 * Parameters:
 *   bits_in:   64-element input array, one uint8 per bit
 *   bytes_out: 8-byte output array (must be pre-zeroed or zeroed by function)
 */
void bits_to_bytes(const uint8_t *bits_in, uint8_t *bytes_out);


/*
 * encrypt
 *
 * High-level DES encryption wrapper.
 * Signature: encrypt(unsigned char* plaintext, unsigned char* key)
 *
 * Confirmed from disassembly at 0x804c7cc in qalfat.
 *
 * Performs in-place DES encryption:
 *   1. bytestobit(plaintext, bit_buf_plain)
 *   2. bytestobit(key, bit_buf_key)
 *   3. des(bit_buf_plain, bit_buf_key, DES_MODE_ENCRYPT)
 *   4. bitstobytes(bit_buf_plain, plaintext)
 *
 * The result is written back into plaintext, overwriting the input.
 * The key buffer is not modified.
 *
 * In the fopimpt encryption sequence (confirmed from disassembly at
 * 0x804ccff), the call is:
 *   encrypt(mk2, mk1)
 * where mk2 at 0x8052a00 is the device-specific key (8 bytes from USA[+0x2d])
 * and mk1 at 0x8052a09 is the session key from the received frame.
 *
 * Parameters:
 *   plaintext: 8-byte buffer to encrypt in place
 *   key:       8-byte DES key (56 effective bits, 8 parity bits ignored)
 */
void encrypt_block(uint8_t *plaintext, const uint8_t *key);


/*
 * Permutation table addresses in the qalfat data segment.
 * These are static data arrays in the binary's .data section.
 * Values confirmed from disassembly of des() at 0x804b196.
 *
 * The tables use standard DES 1-based indexing (values range from 1 to N).
 */
#define QALFAT_PERM1_ADDR   0x8052460u  /* IP  table, 56 entries, initial permutation */
#define QALFAT_PERM2_ADDR   0x80524a0u  /* PC1 table, 56 entries, key permutation 1 */
#define QALFAT_PERM3_ADDR   0x80524e0u  /* PC2 table, 48 entries, key permutation 2 */
#define QALFAT_PERM4_ADDR   0x8052520u  /* FP  table, 64 entries, final permutation */
#define QALFAT_PERM5_ADDR   0x8052560u  /* E   table, 48 entries, expansion function */
#define QALFAT_PERM6_ADDR   0x80527a0u  /* additional permutation table */
#define QALFAT_PERM7_ADDR   0x80527c0u  /* additional permutation table */
#define QALFAT_SBOXES_ADDR  0x80525a0u  /* 8 S-boxes, 64 entries each, 512 total */
#define QALFAT_KEYS_ADDR    0x8054880u  /* expanded key schedule, 16 rounds * 48 bits */


/*
 * FCRC18 interface (declared here; implementation in altclass_crypto.c).
 * See crc16-reconstructed.c for the full implementation of the inner
 * CRC-16/KERMIT accumulator (crc16_kermit_byte).
 */

/*
 * fcrc18
 *
 * The altclass FCRC18 proprietary CRC construction.
 * Fully characterised from disassembly of FCRC18 at 0x8049edf and
 * crc16 at 0x8049e9a in the qalfat binary.
 *
 * Algorithm:
 *   seed = ((~buf[1] & 0xff) << 8) | (~buf[0] & 0xff)
 *   for i in [2, len):
 *     seed = crc16_kermit_byte(buf[i], seed)
 *   seed = crc16_kermit_byte(0, seed)
 *   seed = crc16_kermit_byte(0, seed)
 *   result = ~seed & 0xffff
 *   result = ((result << 8) | (result >> 8)) & 0xffff
 *   return result
 *
 * See crc16-reconstructed.c for the implementation.
 */
uint16_t fcrc18(const uint8_t *buf, uint16_t len);


/*
 * fend_send_append_crc
 *
 * Appends the FCRC18 checksum to a frame buffer and updates the length.
 *
 * Reconstructed from fend_send disassembly at 0x804c5b3 in qalfat.
 *
 * When the frame type byte at buf[0] equals 0x6, the CRC covers the
 * sub-frame starting at buf[1] with length (len - 1). Otherwise the
 * CRC covers the full frame from buf[0] with length len.
 *
 * After computing the CRC, appends:
 *   buf[len]     = (crc >> 8) & 0xff    high byte
 *   buf[len+1]   = crc & 0xff           low byte
 * Returns the new frame length (original len + 2).
 *
 * Parameters:
 *   buf: frame buffer (must have at least 2 bytes of headroom beyond len)
 *   len: current frame length before CRC append
 * Returns:
 *   new frame length after CRC append
 */
uint16_t fend_send_append_crc(uint8_t *buf, uint16_t len);
