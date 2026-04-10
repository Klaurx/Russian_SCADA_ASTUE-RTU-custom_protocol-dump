# altclass Protocol Driver Analysis (progr/qalfat)

## Binary Identity

Binary name: qalfat. Class name: altclass. Source file: qalfat.cc.
Version string recovered from strings section: "[VERINFO] qalfat ver 1.8".
Base class: usodrv, inherited directly, confirmed from the typeinfo symbols
_ZTI8altclass and _ZTS8altclass and the single-inheritance type info structure
_ZTI8altclass containing a pointer to the usodrv typeinfo.

Build date: December 9, 2024, consistent with the main firmware release.

The altclass constructor takes a single unsigned short parameter, matching
the pattern of all other usodrv subclasses and distinguishing it from the
aclass constructor in Aqalpha which takes SUPPORTED_USO_AND_BUF_TYPES.

The altclass implementation is distinct from every other USO field bus driver
in the firmware set in that it embeds a complete DES cipher suite within the
binary itself, applies DES encryption to outbound data frames, and uses a
custom non-standard CRC algorithm called FCRC18 for frame integrity checking.
No other field bus driver in the firmware set implements frame-level
encryption.


## Class Method Table

All methods confirmed from nm with C++ demangling:

```
altclass::altclass(unsigned short)
  constructor, two variants (C1 complete and C2 base object)

altclass::~altclass()
  destructor, three variants (D0 deleting, D1 complete, D2 base object)

altclass::AddUserDataAnalog(iocuso*, iocAnalog*, DEF_ANALOG*, SOST_ANALOG*,
                             unsigned char*, long)
  registers an analog measurement point with the usodrv data model
  iterates a 12-entry table at 0x80523f5 searching for a matching channel
  type byte (loop bound cmp edx, 0xc, meaning up to 12 entries)
  valid channel type range: lea eax,[ebx-0x14] / cmp eax, 0x1d
  this means valid channel types are 0x14 (20) through 0x31 (49)
  uses a modulo-10 computation (imul 0x66666667 / sar 2 pattern for
  division by 10) to determine whether the channel falls in the first or
  second half of a decade grouping, setting a flag at [slot+0x8052ab9]
  or [slot+0x8052aba] accordingly
  five device slots exist with stride 0xa8 bytes each

altclass::AddUserDataUso(iocuso*, DEF_USO*, SOST_USO*, unsigned char*, int)
  registers a USO device slot with the data model

altclass::fsost(unsigned short)
  device state query by channel index
  checks device availability and current communication state

altclass::fzUso()
  USO device zero-reset or initialization
  called during driver startup to clear device state before the first poll

altclass::KorAdr()
  address correction and timing synchronization
  confirmed from disassembly at 0x804f1b0
  iterates the device pointer array at 0x80547a0 (loop bound: device count
  at 0x8052e08 checked at entry, returns immediately if count <= 0)
  for each device slot at index esi, reads the USA struct pointer from
  MADR[esi] at 0x80547a0
  calls usodrv::GetParValue(this, byte[ebx+4], byte[ebx+5], DEF_PAR_VALUE*)
  where bytes at [ebx+4] and [ebx+5] are parameter table indices stored in
  the device configuration block
  after GetParValue, checks the result flag at 0x8054844
  if the flag has bit 0 set, performs floating point operations:
    loads the current address from the DEF_PAR_VALUE result
    converts it to integer using fistp to WORD PTR [ebp-0x12]
    compares the integer against the current address byte at [ebx+0x0]
    if they differ, clears a flag at [ebx+0x104] indexed slot
  computes a timing parameter: reads the device count byte at 0x80523e8,
  multiplies by 0x12c (300), and stores to 0x8052dbc
  this timing parameter is a 300ms period per device used to throttle
  the poll cycle when multiple devices are present
  KorAdr is called at the start of both GetDiscret and GetAnalog before
  any device communication

altclass::GetUsomUSA(iocuso*)
  confirmed from disassembly at 0x8049c3c
  exactly three instructions: load iocuso argument, dereference [iocuso+0x20],
  return the pointer at that offset
  the pointer at iocuso+0x20 is the USA struct for that device
  if the pointer is null (device not registered or handle invalid), the
  caller sees a null return and skips the poll cycle for that slot
  cached at global PRV (0x805478c) by both GetDiscret and GetAnalog
  immediately after the call

altclass::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
  confirmed from disassembly at 0x804f2cc
  entry sequence:
    calls usodrv::FindUso with MSG_GET_PARAM[+0x2] as the device index
    zeroes the return count word at MSG_RETURN_DISCRET[+0x2]
    checks the protocol state word at 0x8052dc0 against 0
    checks the timing counter at 0x8052dbc against 0
    if both are zero, calls KorAdr to resynchronize address and timing
    computes the timing value: device_count * 0x12c (300ms per device)
    stores to 0x8052dbc as a countdown
  device handle resolution:
    calls GetUsomUSA with the iocuso pointer from FindUso
    stores result to PRV at 0x805478c
    if null, sets return count to 0 and returns immediately
  device type validation:
    reads MSG_GET_PARAM[+0x6] as the requested data type code
    checks USA[+0xfe] (busy/lock flag): if non-zero, returns 0 immediately
    computes lea eax,[ecx-0x5a] and checks ax <= 1
    this means only type codes 0x5a and 0x5b are accepted as valid
    type 0x5a corresponds to "closed" relay contact state (closed = 0x5a)
    type 0x5b corresponds to "open" relay contact state (open = 0x5b)
  saturation counter clamping:
    reads USA[+0x103] (saturation counter byte)
    if value > 0x14 (20), clamps it back to 0x14
    if value is negative (sign bit set), returns 0 immediately
  type-specific output:
    for type 0x5b: reads USA[+0x103] saturation counter
      if counter >= 0x0a (10): setge result byte, meaning saturation
      threshold of 10 consecutive non-responses triggers discrete output
    for type 0x5a: reads USA[+0x103]
      if counter <= 0x09: setbe result byte, complementary threshold

altclass::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
  confirmed from disassembly at 0x804f4cc
  entry sequence mirrors GetDiscret: FindUso, GetUsomUSA, KorAdr check
  type code from MSG_GET_PARAM[+0x6] stored in esi
  additional read-enable check: also checks FL_READ at 0x8052dc2 against 0
    all three (state word, timing, FL_READ) must be non-zero to proceed
    without calling KorAdr
  channel type range check:
    lea ebx,[esi-0xc] and cmp bx, 0x25 (37 decimal)
    valid channel types for analog are 0x0c through 0x31 (12 through 49)
    this range is wider than the discrete type range
  for channel types in the lower range (0x0c through 0x31 inclusive):
    reads USA[+0x2a] (a 16-bit current measurement or status word)
    compares against TekUI at 0x8052a60 (the current UI reference value)
    if they differ, goes to the not-ready path
  for channel types above 0x0a (10):
    calls fsost(this, type_code) to check device state
    if fsost returns 0, returns empty result
  scaling coefficient application:
    loads float at USA[+0x11c] into local ebp-0x70 (primary coefficient)
    loads float at USA[+0x120] into local ebp-0x6c (secondary coefficient)
    if both coefficients are zero, skips scaling
    if primary is non-zero and secondary is also non-zero, applies both
    the selection between coefficients uses the unit conversion flag at
    USA[+0x119]: flag = 0 means primary units, flag = 1 means secondary

altclass::fkrt(USA*)
  takes a direct USA struct pointer rather than going through GetUsomUSA
  performs device characterization including model identification
  reads the model type from the USA struct and sets internal capability flags

altclass::fener(unsigned short, unsigned int, double)
  energy calculation for the altclass protocol
  takes a channel index, an unsigned int accumulator value, and a double
  precision engineering coefficient
  computes energy in engineering units from raw counter data

altclass::fjur(char*)
  journal output function
  takes a file path string for the journal output destination
  writes device event data to the specified file

altclass::fsq(TIME_SERVER_KANAL)
  timestamp sequencing
  takes a full BCD timestamp struct by value
  manages the sequence of timestamped poll cycles for change detection

altclass::MakeUsoSpecialBuf(MSG_SPECIAL_BUF*, MSG_SPECIAL_BUF**)
  special buffer routing for non-standard queries
  the base class usodrv::MakeUsoSpecialBuf is the virtual that this
  overrides, meaning all MSG_SPECIAL_BUF requests to altclass devices
  go through this method
```


## Crypto Primitives

### des() at 0x804b196

The des function is a standard DES cipher implementing the full 16-round
Feistel network with IP (initial permutation) and FP (final permutation).

Function signature: des(unsigned char* plaintext, unsigned char* ciphertext,
int mode)

The function operates on bit-expanded representations. The first action is
to call Permutation with the IP table at 0x8052460 (size 0x38 = 56 bits)
to apply the initial permutation. The key schedule is pre-expanded and
stored in the keys array at 0x8054880. Each of the 16 rounds calls Permutation
with the round key from the schedule at [keys + round * 0x30] (48 bits per
round key).

The round scheduling has three exceptional positions confirmed from
disassembly:

```
Round 1 (round index 0): single left rotate (esi=0 or esi=1)
Round 9 (round index 8): single left rotate
Round 16 (round index 15 or 0x10): single left rotate
All other rounds: double left rotate
```

This matches the standard DES key schedule rotation amounts (1,1,2,2,2,2,2,
2,1,2,2,2,2,2,2,1) where rounds 1, 2, 9, and 16 rotate by 1 bit and the
others rotate by 2 bits. The comparison sequence cmp esi, 0x1 / jbe, cmp
ebx, 0x9, cmp ebx, 0x10 confirms this.

The mode parameter at [ebp+0x10] controls key schedule direction: mode=0
means decrypt (standard order), mode=1 means encrypt (forward order). The
key index computation uses lea eax,[eax+eax*2] / shl eax, 0x4 to multiply
the round index by 0x30 (48), giving the offset into the pre-expanded key
schedule.

After 16 rounds, a final Permutation with the FP table at 0x8052520
(size 0x40 = 64 bits) applies the final permutation to the output buffer
at [ebp-0xbc].

The function also performs an internal PC2 permutation at 0x8052560 (size
0x30 = 48) on each round key before the Feistel operation.

Permutation tables recovered from data segment addresses:

```
0x8052460   perm1   IP table, 56 bits, initial permutation
0x80524a0   perm2   PC1 table, 56 bits, key permutation 1
0x80524e0   perm3   PC2 table, 48 bits, key permutation 2
0x8052520   perm4   FP table, 64 bits, final permutation
0x8052560   perm5   E table, 48 bits, expansion function
0x80527a0   perm6   additional permutation table
0x80527c0   perm7   additional permutation table
0x80525a0   sboxes  8 S-boxes, 64 entries each
```


### Permutation() at 0x804b127

Signature: Permutation(unsigned char* output, unsigned char* input,
unsigned char count, unsigned char* table)

This is a general bit permutation function used by des for all permutation
steps. If the input pointer is null, it copies count bits from the output
into a local stack buffer first (implementing an in-place permutation).

The core loop at 0x804b17b:

```
for edx from 0 to count-1:
  eax = table[edx]           load the permutation index
  output[edx] = input[eax-1] apply: output bit edx comes from input bit table[edx]-1
```

The subtraction by 1 (the -0x1 in [ebx+eax*1-0x1]) converts from the 1-based
table indexing used in DES specification to 0-based array indexing. This
confirms the permutation tables are stored in standard DES 1-based form.


### SBoxes() at 0x8049de2

Signature: SBoxes(unsigned char* output, unsigned char* sbox_input,
unsigned char* sbox_table)

Processes one 6-bit S-box input block and writes the 4-bit output. The
6-bit input is assembled from the bit array at sbox_input as:

```
edx = sbox_input[0] << 5         row high bit (outermost bits)
edx |= sbox_input[4]             row low bit
edx |= sbox_input[3] << 1        column bit 0
edx |= sbox_input[2] << 2        column bit 1
edx |= sbox_input[1] << 3        column bit 2
edx |= sbox_input[5] << 4        column bit 3
```

This constructs the standard DES S-box row and column index:
row = (bit0 << 1) | bit5, column = bits 1 through 4.

The 4-bit output is read from sbox_table[edx] and then unpacked:

```
output[3] = result & 0x1           LSB
output[2] = (result >> 1) & 0x1
output[1] = (result >> 2) & 0x1
output[0] = (result >> 3) & 0x1   MSB
```


### Xor() at 0x8049d66

Signature: Xor(unsigned char* target, unsigned char* source, unsigned char count)

Performs a count-byte XOR operation: target[i] ^= source[i] for i from 0
to count-1. Used in the DES round function to XOR the round key with the
expanded half-block.


### Copy() at 0x8049da4

Signature: Copy(unsigned char* dest, unsigned char* src, unsigned char count)

Simple byte array copy: dest[i] = src[i] for i from 0 to count-1.
Used in the DES implementation to copy half-blocks between rounds.


### bytestobit() at 0x8049e4b

Signature: bytestobit(unsigned char* bytes_in, unsigned char* bits_out)

Expands a byte array to a per-bit representation. Iterates 64 positions
(cmp esi, 0x40 confirming 64-bit block). For each bit position, computes
the byte index as esi/8 and the bit mask using (7 - (esi % 8)) as the shift.
Sets bits_out[esi] to 1 if the corresponding bit in bytes_in is set, 0
otherwise.


### bitstobytes() at 0x804c768

Signature: bitstobytes(unsigned char* bits_in, unsigned char* bytes_out)

Collapses a per-bit array back to byte form. First clears the output buffer
with memset(bytes_out, 0, 8). Then iterates 64 positions (cmp edi, 0x40).
For each bit at bits_in[edi] that is set, ORs the corresponding bit into
the output byte at bytes_out[edi/8], using (7 - (edi % 8)) as the bit
position within the byte.


### encrypt() at 0x804c7cc

Signature: encrypt(unsigned char* plaintext, unsigned char* key)

The entry-level encryption wrapper that puts the full DES operation together:

Step 1: bytestobit(plaintext, bit_buf_plain) expands plaintext to 64 bits.
Step 2: bytestobit(key, bit_buf_key) expands the key to 64 bits.
Step 3: des(bit_buf_plain, bit_buf_key, 1) performs DES encryption
  (mode=1 for encrypt).
Step 4: bitstobytes(bit_buf_plain, plaintext) collapses the result back
  to bytes in place.

The result is written back into the plaintext buffer, making this an
in-place encryption operation.


## FCRC18 Algorithm

### Inner crc16() at 0x8049e9a

Signature: crc16(unsigned char byte, unsigned short crc_in)

This is CRC-16/KERMIT (also known as CRC-16/CCITT-FALSE with bit reversal).
The polynomial is 0x8408, which is the bit-reflected form of 0x1021.

The bit loop confirmed from disassembly at 0x8049ea5 through 0x8049ebe:

```
crc = crc_in
for 8 iterations:
  if crc & 0x0001:
    crc = (crc >> 1) ^ 0x8408
  else:
    crc >>= 1
return crc
```

This is CRC-16/KERMIT: initial value fed from outside, polynomial 0x8408,
input reflected (processed LSB first), output reflected (by construction
of the reflected polynomial), no output XOR. The polynomial 0x8408 is the
reflected form of 0x1021 used by CRC-16/CCITT.


### FCRC18() at 0x8049edf

Signature: FCRC18(unsigned char* buf, unsigned short len)

This algorithm uses crc16 as its accumulator but applies a non-standard
three-layer transformation:

Layer 1: seed construction from the first two bytes.

```
high_byte = ~buf[1] & 0xff    bitwise NOT of buf[1], masked to 8 bits
low_byte  = ~buf[0] & 0xff    bitwise NOT of buf[0], masked to 8 bits
seed = (high_byte << 8) | low_byte
```

The seed is the byte-swapped bitwise complement of the first two bytes of
the buffer. This is confirmed from the disassembly sequence:
movzx edx from buf[0], movzx eax from buf[1], NOT both (via xor with 0xff
or sub eax, 1 then NOT), shift one left 8, OR together.

Layer 2: main accumulation loop.

```
checksum = seed
for i from 2 to len-1 inclusive:
  checksum = crc16(buf[i], checksum)
```

The loop starts at index 2, skipping the first two bytes that were used
to construct the seed. This means the seed already encodes buf[0] and buf[1]
in inverted form, and the loop accumulates bytes 2 through len-1 using
CRC-16/KERMIT.

Layer 3: zero padding and output transformation.

```
checksum = crc16(0x00, checksum)    feed first zero byte
checksum = crc16(0x00, checksum)    feed second zero byte
result = ~checksum & 0xffff         bitwise NOT of final checksum
result = ((result << 8) | (result >> 8)) & 0xffff    byte-swap
return result
```

The double zero feed at the end is a deliberate protocol design choice
that ensures the CRC covers a defined minimum number of bytes even for
very short frames. The final NOT and byte-swap are also deliberate, ensuring
the output cannot be mistaken for a standard KERMIT CRC value.


### How FCRC18 is Applied to Frames

Confirmed from fend_send disassembly at 0x804c5b3.

When fend_send is called with argument 1 (transmit mode):

Step 1: reads the type byte at BufSend[0] (0x8052ec0) against 0x6.

  If type byte is 0x6:
    calls FCRC18(buf+1, KolSend-1)
    this covers the sub-frame starting at byte 1, skipping the type byte

  If type byte is not 0x6:
    calls FCRC18(buf, KolSend)
    this covers the full frame from byte 0

Step 2: writes the CRC result into the buffer.

```
buf[KolSend]     = (crc >> 8) & 0xff    high byte
buf[KolSend+1]   = crc & 0xff           low byte
KolSend += 2
```

Step 3: determines the inter-frame gap based on SHP (0x8052ea2):

  If SHP == 0x40: ebx = 0x14 (20 ms gap), esi = 5 (5 retries)
  If SHP == 0x14: ebx = 0x14, esi = 3
  Otherwise:      ebx = (SHP != 0x14) ? 0x19 : 0x14, esi = 3

Step 4: reads the device count from 0x80523e8, stores to TIC (0x8052db0).

Step 5: if KolSend > 0, calls write(fd, BufSend, KolSend).

  The file descriptor comes from 0x8052df8.
  If write returns != KolSend (partial or failed write): TIC = 0 and
  the function enters the error recovery path.

Step 6: if write succeeded, copies KolPr to KolPriem (base counter advance),
  then calls readcond(fd, recv_buf, ...) with:
    min_bytes = KolPr+KolSend as the start offset into BufPriem
    max_bytes = 0x1f4 (500)
    timeout_min = ebx (the inter-frame gap computed above)
    timeout_max = esi (the retry count)
  readcond is the QNX non-blocking serial read with timeout parameters.


## fopimpt Session State Machine

### Thread Entry at 0x804c832

The fopimpt function is the main working thread for the altclass protocol.
It is spawned by StartDrv() as the primary working thread. The thread entry
performs the following initialisation sequence:

```
getpid() stored to pid1 at 0x8054850
ConnectAttach(0, pid1, chida, 0, 0) called with:
  node    = 0 (local node)
  pid     = pid1
  chid    = chida at 0x8052dd8
  index   = 0
  flags   = 0
result stored to coid1 at 0x8054854
SHP (session high/protocol byte) at 0x8052ea2 = 0x00
SOST_KAN (session state) at 0x8052ea0 = 0x00
NPT (no-protocol flag) at 0x8052eaa = 0x00
FL_R (read enable) at 0x8052ea1 = 0x00
```

The receive buffer pointer pbuf at 0x805457c is set to point into BufPriem
at 0x8053000 offset by the current BZ (buffer zone) counter.


### Main Loop Gate

The main loop at 0x804c89e begins by checking two global flags:

```
if SHP (0x8052ea2) != 0: goto state machine dispatch at 0x804cb10
if SOST_KAN (0x8052ea0) != 0: goto state machine dispatch at 0x804cb10
```

If both are zero, the loop sends a MsgSend to the service channel, passing
a 4-byte message at Msgsnd (0x8054868) and expecting a 4-byte reply at
Msgotv (0x8054858). This is the idle-state heartbeat: the thread signals
its service channel that it is alive and waiting.

After the MsgSend:

  checks FL_DEV (0x8054610) against 0: if set, skip port acquisition
  checks a device availability flag at bufsh (0x8054638)[0] against 1:
    if not 1, skip port acquisition
  checks Flag (0x8054580) against 1:
    if 1, calls usodrv::FreePort to release the serial port

Clears Flag to 0.

Reads the device count from 0x80523e8, multiplies by 0x1e (30), stores
to TIC (0x8052db0) as the poll period base.

Clears FL_DC (0x8052dc0) to 0.

Calls delay(0) to yield the CPU before the next iteration.


### Port Acquisition

When the MsgSend returns and both SHP and SOST_KAN are still zero:

  if Flag is 0:
    calls usodrv::WaitKanalFree(this, fd, port_num) to wait for the
    serial port to be free from other users
    calls InitPort() to configure the serial port termios parameters
    if InitPort fails: calls usodrv::FreePort and restarts the idle loop
    if InitPort succeeds: calls usodrv::FreePort for cleanup after init

Clears MADR pointer at 0x805478c (the currently active device handle).

Sets up the device address: reads FL_R and uses it to index into MADR
at 0x80547a0 to load the USA struct pointer for the current device slot.

Stores the USA pointer to both AdrUso at 0x8054788 and PUSRV at 0x8054784.

Computes the AconfK offset: AdrUso + 0x124 stored to Aconf at 0x8054640.
The 0x124 offset within the USA struct points to the configuration subblock.


### State Machine Dispatch

After the idle loop gate, the session state byte at SHP (0x8052ea2) drives
a jump table at 0x8050ca8. The maximum valid state is 0x48 (72 decimal).
States above 0x48 jump to the error recovery path at 0x804ec39.

The state machine implements the following major phases:

State 0x00 through 0x08 (initialisation phase):

  State 0x00 entry: sets the device identifier at 0x8052a1b to 0, sets
  SHP to 0x20 (initial handshake state), sets icr flag at 0x8052a31 to 1.
  Loads the current device address from FL_R index into MADR.
  Checks FL_DC (0x8052dc0) against 4: if equal, jumps to state 0x40 path.
  Checks NumDay (0x8052dac) against 0: if zero, goes to the first-contact
  path at 0x804cac3.
  Checks MADR's address byte at [AdrUso+0x39] against 0: if zero, jumps
  to the end of the state machine (device not configured).

State 0x14 (data exchange state):

  This is the normal operating state after handshake completion.
  Calls Read_OK181() to check for received data.
  Stores result to sh (0x8052eae).
  Compares against 0x6 and 0x1 (both are success codes):
    on success (0x6 or 0x1): increments the retry counter at PUSRV[+0x100]
    if retry counter was <= 1: resets KolSend to 0 and calls fend_send(0)
    fend_send(0) is the clean-close path, advancing the base pointer

State 0x16 (post-encryption transmit state):

  After a successful encrypt call, fopimpt sets SHP to 0x16.
  This state performs the authenticated frame transmission:
    sets icr flag at 0x8052a30 to 1 (indicating encrypted content)
    loads pbuf from BZ-indexed BufPriem
    calls fend_send(1) to append FCRC18 and transmit
  Returns to main loop.

State 0x40 (long-timeout idle):

  Entered when FL_DC (0x8052dc0) equals 4, meaning the device count
  is large enough to require an extended inter-poll delay.
  Checks the timing counter and resets FL_DC if the count is exhausted.
  Computes extended timing: device_count * 0x15180 (86400, one day in
  seconds), stored to ticui at 0x8052e9c.
  Jumps to the end of the state machine (0x804ec94) which performs the
  next-device advance.


### Encryption Sequence

The encryption sequence within fopimpt occurs at the transition between
receiving a valid response and transmitting the authenticated reply. Confirmed
from the disassembly at 0x804ccc0 through 0x804cd74:

Step 1: the long-frame path (KolPriem > 0x18 = 24 bytes) is selected.

Step 2: load the frame type discriminator from the receive buffer at
  [BZ + BufPriem + 0xd] (offset 13 into the received frame) against 0x6.
  If 0x6, select the sub-frame CRC path.
  If not 0x6, select the full-frame path.

Step 3: set the busy flag PUSRV[+0xfe] to 1.

Step 4: reset the retry counter PUSRV[+0x100] to 0.

Step 5: check PUSRV[+0x102] state byte against 0x32 (50 decimal).
  If equal to 0x32, preserve it (this is a special device state code).
  Otherwise reset to 0.

Step 6: copy 8 bytes from [pbuf+KolPr+1] (the key material from the received
  frame) into mk1 at 0x8052a09.

Step 7: copy 8 bytes from [PUSRV+0x2d] (the device-specific key offset +0x2d
  within the USA struct, confirmed from the movzx eax, BYTE PTR [eax+edx*1+0x2d]
  at 0x804ccdb) into mk2 at 0x8052a00.

Step 8: call encrypt(mk2, mk1) to encrypt the device key block using the
  received session key. This produces the authenticated response key.

Step 9: reset KolPr (base pointer) and KolPriem (receive position) to 0.

Step 10: set rpor at 0x8052a2c to 0x100 (256), the maximum response size.

Step 11: set a local byte at [ebp-0x19] to 0x8 (8 bytes, the DES block size).

Step 12: call fzapr(1, 0x60, &local_byte, 0) to build the outbound request
  frame of type 1 with maximum length 0x60 (96 bytes) and 8-byte key field.

Step 13: set SHP to 0x16 (post-encryption transmit state).


## USA Struct Layout

The USA struct is the central device state block for the altclass protocol.
It is accessed via iocuso+0x20 and its pointer is cached in PRV (0x805478c).
All field offsets are confirmed from disassembly of GetDiscret, GetAnalog,
KorAdr, ftek18, and the fopimpt state machine.

```
Offset   Type       Name              Source              Confidence
+0x00    uint8      device_addr       [ebx+0x0] in KorAdr confirmed
+0x20    ptr        usa_ptr           iocuso[+0x20]       confirmed dereference
+0x2a    uint16     current_ui        [eax+0x2a]          confirmed GetAnalog
+0x2c    uint8      mode_type         [edx+0x2c] cmp 1,2  confirmed
+0x2d    uint8[8]   device_key        [eax+edx+0x2d]      confirmed encrypt step
+0x36    uint16     channel_bitmask   [eax+0x36]          inferred from analog
+0x38    uint8      comms_ok_flag     [eax+0x38] cmp 0    confirmed state check
+0x39    uint8      device_address    [eax+0x39] cmp 0    confirmed KorAdr
+0x78    uint32[11] time_components   [eax+0x78..+0xa0]   localtime fields
+0xfe    uint8      busy_lock         [edx+0xfe] cmp 0    confirmed GetDiscret
+0x100   uint16     retry_counter     [eax+0x100]         confirmed fopimpt
+0x102   uint8      state_byte        [eax+0x102] set 0,4 confirmed fopimpt
+0x103   uint8      saturation_cnt    [eax+0x103] cap 0x14 confirmed GetDiscret
+0x104   uint8      slot_flag         [ebx+eax+0x104]     confirmed KorAdr
+0x114   uint16     config_word_a     [eax+0x114]         inferred
+0x116   uint16     config_word_b     [eax+0x116]         inferred
+0x119   uint8      unit_conv_flag    [eax+0x119]         confirmed ftek18
+0x11c   float      coeff_primary     fld [eax+0x11c]     confirmed GetAnalog
+0x120   float      coeff_secondary   fld [eax+0x120]     confirmed GetAnalog
+0x124   varies     config_subblock   AdrUso+0x124=Aconf  confirmed fopimpt
```

The type codes 0x5a and 0x5b accepted by GetDiscret correspond to substation
relay contact states. The value 0x5a in Cyrillic/KOI8 encoding is not a
standard ASCII character but the numeric values strongly suggest these are
protocol-defined codes for the two complementary contact states. In the
USOTM protocol, 0x5a is the new-format analog type byte, which may indicate
a shared protocol heritage between the USOTM and altclass protocol families.


## ftek18 Measurement Scaling

The ftek18 function (at 0x804adb6) is a measurement scaling dispatcher,
not a frame builder.

It processes up to 5 device slots (loop bound cmp ecx, 0x5), with stride
0xa8 bytes between slots. Inner initialisation clears 0x11 (17) bytes per
slot.

After clearing, it reads scaling data from the device configuration and
applies floating point multiplication against a constant at 0x8050f74.

Unit conversion flag check: reads USA[+0x119] and branches:
  if flag == 0: multiplier comes from 0x8050f88 (primary units)
  if flag != 0: multiplier comes from 0x8050f90 (secondary units)

The type discriminator range check is lea eax,[edx-0x15] / cmp ax, 0x1c,
meaning valid device type bytes for ftek18 are 0x15 (21) through 0x31 (49).

Output: the computed float value is stored to a global float array at
0x8052a64, indexed by channel: fstp DWORD PTR [eax*4+0x8052a64]. This array
holds the current scaled measurement values for all active channels.


## Global State Variables

The altclass protocol maintains its session state in a dense set of globals
at addresses from 0x8052a00 through 0x8054870. The most operationally
significant globals:

```
0x8052a00   mk2       8-byte device key buffer for encrypt input
0x8052a09   mk1       8-byte session key buffer from received frame
0x8052a1a   TR        transmit ready flag
0x8052a1b   RB        read-back or re-transmit flag
0x8052a1c   reboot    reboot request flag
0x8052a1e   fltim     filter timer value
0x8052a20   RBL       read-back length
0x8052a22   RB        secondary read-back flag
0x8052a24   Rl        response length
0x8052a26   tind      timing indicator
0x8052a28   MaxBl     maximum block size
0x8052a2a   Np        number of packets
0x8052a2c   rpor      response port or maximum response size (0x100)
0x8052a2e   Np2       secondary packet count
0x8052a30   icr       encrypted content flag
0x8052a31   klp       key length parameter
0x8052a34   sostbuf   state buffer
0x8052a44   mlz       minimum length zero flag
0x8052a60   TekUI     current UI reference value
0x8052a64   TekUI+4   start of scaled measurement float array
0x8052dbc   ticinit   timing countdown (device_count * 300ms)
0x8052dc0   FL_DC     device count state (0,1,4 are valid states)
0x8052dc2   FL_READ   read enable flag
0x8052db0   TIC       poll period timer
0x8052ea0   SOST_KAN  session state (channel state)
0x8052ea1   FL_R      read index for device slot selection
0x8052ea2   SHP       session high protocol byte (state machine state)
0x8052ea3   KolUsrv   number of active service connections
0x8052ea4   KolSend   current send buffer length
0x8052ea6   KolPriem  current receive position counter
0x8052eaa   NPT       no-protocol flag
0x8052eac   KolPr     receive base counter
0x8052eae   sh        result from Read_OK181
0x8052eb0   MTIP      maximum time in poll
0x8052ec0   BufSend   outbound frame buffer (320 bytes)
0x8052e00   nomindex  device slot index
0x8052dfc   nomport   serial port number
0x8052df8   fdp       file descriptor for the serial port
0x8053000   BufPriem  inbound frame buffer (start address)
0x8054580   Flag      port acquisition flag
0x8054582   BZ        buffer zone index (offset into BufPriem)
0x805457c   pbuf      pointer into current receive buffer position
0x8054610   FL_DEV    device available flag
0x8054638   bufsh     device availability struct pointer
0x8054784   PUSRV     USA struct pointer for primary device
0x8054788   AdrUso    USA struct pointer (current active device)
0x805478c   PRV       cached GetUsomUSA result
0x8054848   us        usodrv this pointer
0x8054850   pid1      process ID of fopimpt thread
0x8054854   coid1     connection ID to service channel
0x8054880   keys      DES expanded key schedule (48 bytes * 16 rounds)
0x80547a0   MADR      array of USA struct pointers, one per device slot
```
