# TestPriem() Frame Validation Analysis

## Function Signature

```
cusotm::TestPriem(uint8_t* buf, uint16_t len) -> int
```

Address in usotm binary: 0x804971a

## Purpose

Validates a received USOTM field bus frame. Returns 1 if the frame passes
all checks, 0 if any check fails. Called by every Raspak parsing function
(RaspakDiscret, RaspakAddAnalog, RaspakImpuls, RaspakTempVn, RaspakTempNar,
RaspakAnalog, RaspakOldAnalog) before any payload parsing begins. Failure
causes the caller to invoke AddError(uso_index) and return 0 immediately
without touching the payload bytes.

## Disassembly Walkthrough

The function entry establishes its frame, then immediately checks the
minimum length:

```
8049727:  cmp di, 0x5
804972b:  jbe 804979b     if len <= 5, return 0 (too short, invalid)
```

Minimum valid frame is 6 bytes. A frame of exactly 6 bytes carries zero
payload items and consists only of the 4-byte header and the 2-byte CRC.

```
8049730:  mov [ebp-0x10], eax   save len to stack
8049733:  mov esi, eax
8049735:  sub esi, 0x2          esi = len minus 2, used as the loop bound
8049738:  mov ecx, 0x0          ecx holds the running checksum, initialised to 0
804973d:  mov ebx, 0x1          ebx is the byte index, starts at 1 (skips start byte)
```

The checksum loop iterates from buf[1] to buf[len-3] inclusive. Note that
buf[0] (the start byte) is excluded from the CRC calculation, and buf[len-2]
and buf[len-1] (the CRC bytes themselves) are also excluded.

```
8049747:  movzx eax, cx              zero-extend low 16 bits of checksum to eax
804974a:  mov [esp+0x4], eax         arg1 = current checksum value
804974e:  movzx eax, BYTE [edx+ebx]  arg0 = buf[ebx], the current byte
8049755:  call DWORD PTR [ecx+0x11c] vtable call: accumulate(buf[i], checksum)
8049761:  mov ecx, eax               new checksum returned in eax, saved to ecx
8049763:  add ebx, 1
8049766:  cmp ebx, esi               while ebx < len-2
8049768:  jl  8049747
```

The accumulation function is a virtual method at vtable offset 0x11c. Its
inputs are the current byte value and the running checksum. Its output is the
updated checksum. The result is a 16-bit value used as a two-byte CRC.

Two candidate algorithms are implemented in crc16-reconstructed.c:

  Modbus CRC16 using the polynomial 0xa001 (reflected 0x8005). This is
    confirmed used by sirius_mb and mdbf via the auchCRCHi and auchCRCLo
    symbol names and the exported CRC_Update(uint8_t*, uint16_t) function.

  CRC16 CCITT using the polynomial 0x1021 (unreflected). This is a common
    alternative for serial protocols in embedded systems.

A separate CRC function MCRC16 appears in the Aqalpha, qcet, and qpuso
binaries. A function RC16 appears in cicpcon. A function FCRC16 appears in
Aqalpha and qpuso. The multiplicity of CRC functions suggests different
protocol layers use different algorithms, and the USOTM accumulator may not
match any of them.

After the loop, the checksum is split and compared against the frame tail:

```
804976f:  shr dx, 0x8           extract high byte of checksum into dx
8049776:  movzx eax, BYTE [esi+ebx-0x2]  load buf[len-2]
804977b:  cmp dx, ax            compare high byte
804977e:  jne 804979b           mismatch: return 0

8049780:  movzx edx, cl         extract low byte of checksum into edx
8049783:  movzx eax, BYTE [esi+ebx-0x1]  load buf[len-1]
8049788:  cmp edx, eax          compare low byte
804978a:  jne 804979b           mismatch: return 0
```

The CRC is stored high byte first at buf[len-2] and low byte second at
buf[len-1]. This is the standard network byte order for a 16-bit checksum.

Final length consistency check:

```
804978c:  movzx eax, BYTE [esi+0x3]  load buf[3], the payload item count N
8049790:  add eax, 0x6               compute expected length: N + 6
8049793:  cmp eax, [ebp-0x10]        compare with actual length
8049796:  sete al                    set al to 1 if equal, 0 if not equal
```

The function returns 1 (valid) only if all three conditions pass: minimum
length satisfied, CRC matches, and the payload count is consistent with
the total frame length.

## Frame Layout Derived from Length Formula

The equation total_len = buf[3] + 6 establishes the following layout:

```
Offset   Size       Field
0        1          Start byte, validated by IsFirstByte before TestPriem is called
1        1          Response type byte, identifies the parser to invoke
2        1          Device address, compared against tag[+0x0f] in each parser
3        1          Payload item count N
4        N*stride   Payload items, stride and encoding depend on the type byte
len-2    1          CRC high byte, (checksum >> 8) and 0xff
len-1    1          CRC low byte, checksum and 0xff
```

Total fixed overhead is 6 bytes: 4 header bytes plus 2 CRC bytes. Payload
is N items whose individual encoding depends on the response type byte.

For discrete frames with type 0x57, N is the raw byte count, and the actual
word count is derived by the formula in RaspakDiscret:
  word_count = (N * 0xab >> 8) >> 2

For temperature frames with type 0x5d, N must equal 2 (giving total length
of exactly 8 bytes), confirmed by the si == 0x8 check in RaspakTempVn.

For impulse frames with type 0x56, N is the raw payload byte count and the
actual active counter count is derived from the bitmask in bytes 4 through 7.

For analog frames with type 0x5a, N is the raw payload byte count and the
actual active channel count is the sum of set bits across bytes 4 through 7,
computed by calling KolBits on each byte separately.

## Return Values

```
0   invalid: frame too short, CRC mismatch, or length inconsistency
1   valid: all three checks passed
```

## Callers

Every Raspak function in cusotm calls TestPriem as its first action:

  RaspakDiscret at 0x804a38c
  RaspakAddAnalog at 0x804b0e6
  RaspakImpuls at 0x804b2ae
  RaspakTempVn at 0x8049e54
  RaspakTempNar at 0x8049d12
  RaspakAnalog at 0x804ad62
  RaspakOldAnalog at 0x804ac0e

All callers share the same failure pattern: call AddError(uso_index) and
return 0 immediately without parsing any payload bytes.

## Note on ckpris::TestPriem

The ckpris class (KPRIS protocol driver) has its own TestPriem method at
0x804b052 with a different signature:

  ckpris::TestPriem(unsigned char, unsigned char*,
                    unsigned short&, unsigned char, unsigned char)

This five-argument version is completely independent of cusotm::TestPriem.
It validates KPRIS protocol frames using KPRIS-specific framing rules. The
two TestPriem implementations share no code and validate different frame
formats. Earlier documentation did not distinguish between them.
