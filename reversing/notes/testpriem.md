# TestPriem() Analysis

## Function Signature

```
cusotm::TestPriem(uint8_t* buf, uint16_t len) -> int
```

Address in usotm binary: 0x804971A

## Purpose

Validates a received USOTM field bus frame. Returns 1 if valid, 0 if invalid.
Called by RaspakDiscret, RaspakAddAnalog, RaspakImpuls, RaspakTempVn before
any payload parsing begins.

## Disassembly Walkthrough

```
8049727:  cmp di, 0x5
804972B:  jbe 804979B       ; len <= 5: return 0 (invalid, too short)
```

Minimum valid frame is 6 bytes.

```
8049730:  mov [ebp-0x10], eax   ; save len
8049733:  mov esi, eax
8049735:  sub esi, 0x2          ; esi = len - 2 (loop bound)
8049738:  mov ecx, 0x0          ; ecx = running checksum
804973D:  mov ebx, 0x1          ; ebx = byte index (starts at 1)
```

The checksum loop iterates from buf[1] to buf[len-3] inclusive:

```
8049747:  movzx eax, cx              ; low 16 of checksum
804974A:  mov [esp+0x4], eax         ; arg1 = current checksum
804974E:  movzx eax, BYTE [edx+ebx]  ; arg0 = buf[ebx]
8049755:  call DWORD PTR [ecx+0x11C] ; vtable call: accumulate(buf[i], checksum)
8049761:  mov ecx, eax               ; new checksum in ecx
8049763:  add ebx, 1
8049766:  cmp ebx, esi               ; while ebx < len-2
8049768:  jl  8049747
```

The accumulation function is a virtual method at vtable offset 0x11C. Its
exact polynomial is unconfirmed but the result is a 16-bit value used as a
two-byte CRC.

After the loop, the result is split and compared:

```
804976F:  shr dx, 0x8              ; high byte of checksum
8049776:  movzx eax, BYTE [esi+ebx-0x2]  ; buf[len-2]
804977B:  cmp dx, ax               ; high byte match?
804977E:  jne 804979B              ; no: return 0

8049780:  movzx edx, cl            ; low byte of checksum
8049783:  movzx eax, BYTE [esi+ebx-0x1]  ; buf[len-1]
8049788:  cmp edx, eax             ; low byte match?
804978A:  jne 804979B              ; no: return 0
```

Final length check:

```
804978C:  movzx eax, BYTE [esi+0x3]  ; buf[3] = payload item count N
8049790:  add eax, 0x6              ; expected len = N + 6
8049793:  cmp eax, [ebp-0x10]       ; compare with actual len
8049796:  sete al                   ; return 1 if equal, 0 otherwise
```

## Frame Layout Derived from Length Formula

The equation len = buf[3] + 6 implies:

```
Offset   Size       Field
0        1          Start byte (checked by IsFirstByte before TestPriem)
1        1          Response type byte (0x57, 0x56, 0x5D, 0x86)
2        1          Device address
3        1          Payload item count N
4        N * ?      Payload (stride depends on type)
len-2    1          CRC high byte
len-1    1          CRC low byte
```

Total overhead is 6 bytes (4 header + 2 CRC). Payload is N items whose
individual size depends on the response type.

## Return Values

```
0   invalid (short frame, CRC mismatch, length mismatch)
1   valid
```

## Callers

Every Raspak* function calls TestPriem as its first action and calls
AddError(uso_index) then returns 0 on failure without parsing payload.
