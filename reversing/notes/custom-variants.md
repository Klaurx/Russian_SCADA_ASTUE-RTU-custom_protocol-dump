# USOM Field Bus Protocol Analysis (usom and usom2)

## Overview

The cusom class implements a third distinct USO field bus class, separate
from both cusotm (USOTM field bus) and the RTU class polling drivers. The
source files are usom.cc and usom2.cc. Both binaries use /dev/ser1 as the
default serial port. The class name cusom (as opposed to cusotm) suggests
a different device family, possibly USOM (Unified Serial Output Module) or
a similar Russian abbreviation.

The error string "Error USOM No Mewmory" (with the misspelling of Memory
preserved from the original source) appears identically in both usom and
usom2, confirming they share the same error handling code path for memory
allocation failures.

Both binaries link against libservdrv.so.1, libsystypes.so.1, and
libusodrv.so.1. Both use pthread_cond_signal, pthread_cond_wait,
pthread_mutex_lock, and pthread_mutex_unlock for producer/consumer
synchronization. The global mutex mtx and condition variable cond appear
at data segment addresses in both binaries, at slightly different absolute
addresses due to binary size differences but at the same relative positions.

usom also calls tcdrain (confirmed from the undefined symbol table in the
usotmj output, which shares the same pattern). usom calls sleep rather than
delay for longer waits, while usom2 uses both sleep and close (for port
reset when the error counter threshold is exceeded).


## usom Method Table

All methods confirmed from nm of the usom binary:

```
cusom::cusom(unsigned short)
  constructor, two variants

cusom::~cusom()
  destructor, three variants

cusom::AddDoutInQuery(unsigned char, unsigned short, unsigned char,
                      unsigned short)
  queues a discrete output command with 4 parameters:
    flag byte, address word, mode byte, value word
  this is the 4-parameter variant distinct from the 5-parameter usom2 version

cusom::AddError(int)
  increments the error counter for device at index int

cusom::AddKod(int, int, int)
  stores a measurement value: device_index, channel_index, value

cusom::AddOK(int)
  marks device at index int as responding successfully

cusom::AddUserDataAnalog(iocuso*, iocAnalog*, DEF_ANALOG*, SOST_ANALOG*,
                          unsigned char*, long)
  registers an analog measurement point

cusom::AddUserDataImpuls(iocuso*, iocImpuls*, DEF_IMPULS*, SOST_IMPULS*,
                          unsigned char*, long)
  registers an impulse counter point

cusom::AddUserDataUso(iocuso*, DEF_USO*, SOST_USO*, unsigned char*, int)
  registers a USO device slot

cusom::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
  analog value retrieval

cusom::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
  discrete state retrieval

cusom::GetImpuls(MSG_GET_PARAM*, MSG_RETURN_IMPULS*)
  impulse counter retrieval

cusom::GetSostUso(unsigned short)
  device state query by slot index, calls FindUso then GetUsomTag

cusom::GetUsomTag(iocuso*)
  retrieves the device tag struct pointer from the iocuso handle
  the tag struct is at a different offset from the USA struct used in
  the altclass and aclass families

cusom::InitPort()
  configures the serial port with 8N1 termios settings

cusom::IsFirstByte(unsigned char)
  validates the first received byte before frame accumulation

cusom::KolBits(unsigned short)
  counts the number of set bits in a 16-bit value, same algorithm as cusotm

cusom::RaspakAnalog(int, unsigned char*, unsigned short)
  parses an analog response frame

cusom::RaspakDiscret(int, unsigned char*, unsigned short)
  parses a discrete response frame

cusom::RaspakImpuls(int, unsigned char*, unsigned short)
  parses an impulse counter response frame

cusom::RaspakTemp(int, unsigned char*, unsigned short)
  parses a temperature response frame

cusom::SendBuffer(unsigned char*, unsigned short)
  transmits a frame to the field device

cusom::SendSbrosImpuls(int)
  sends an impulse counter reset command

cusom::SendSbrosLatch(int)
  sends a discrete latch reset command after successful RaspakDiscret

cusom::SendTuCommand(unsigned char, unsigned short, unsigned char, unsigned short)
  sends a single telecontrol command with 4 parameters (4-parameter variant)

cusom::SendTuFromQuery()
  drains queued telecontrol commands

cusom::SendTu(int)
  sends a group telecontrol command

cusom::SetDout(MSG_SET_DOUT*, MSG_RETURN_DOUT*)
  discrete output set from SCADA command

cusom::TestPriem(unsigned char*, unsigned short)
  frame validation, same three-check structure as cusotm::TestPriem

cusom::WaitOtvet(unsigned char*, unsigned short)
  receive loop with timeout

cusom::Working()
  main per-device poll cycle thread

cusom::ZaprosImpuls(int)
  sends an impulse counter poll request

cusom::ZaprosTemp(int)
  sends a temperature poll request

cusom::ZaprosTi(int)
  sends a teleindication (analog measurement) poll request

cusom::ZaprosTs(int)
  sends a telestatus (discrete state) poll request
```

The usom binary does not include MakeUsoSpecialBufZaprosUso. This method is
present only in usom2 among the cusom family. The base class
usodrv::MakeUsoSpecialBufZaprosUso is listed as undefined in usom, meaning
usom uses the base class virtual dispatch for special buffer routing without
overriding it. The usom2 binary overrides it with the three-instruction
passthrough (store input to output pointer, return 1).


## usom2 Additions and Differences

### AddDoutInQuery Signature Change

usom (cusom) signature: AddDoutInQuery(unsigned char, unsigned short,
unsigned char, unsigned short)
Four parameters: flag byte, address word, mode byte, value word.

usom2 (cusom) signature: AddDoutInQuery(unsigned short, unsigned char,
unsigned short, unsigned char, unsigned short)
Five parameters: extended address or slot word, flag byte, address word,
mode byte, value word.

The additional leading unsigned short in usom2 provides a wider address
space or slot specifier, suggesting usom2 supports more than 255 discrete
output points per device, or supports a hierarchical addressing scheme where
the first word identifies a logical sub-device.


### SendTuCommand Signature Change

usom (cusom) signature: SendTuCommand(unsigned char, unsigned short,
unsigned char, unsigned short)
Four parameters matching AddDoutInQuery.

usom2 (cusom) signature: SendTuCommand(unsigned short, unsigned char,
unsigned short, unsigned char, unsigned short)
Five parameters matching AddDoutInQuery, with the same leading word.

This change is consistent: AddDoutInQuery feeds SendTuCommand, so both
must share the same address width.


### MakeUsoSpecialBufZaprosUso

Confirmed from disassembly of 0x8049b12 in usom2. The function body is
exactly five instructions:

```
push ebp
mov ebp, esp
mov edx, DWORD PTR [ebp+0x10]   load output pointer-to-pointer argument
mov eax, DWORD PTR [ebp+0xc]    load input MSG_SPECIAL_BUF pointer
mov DWORD PTR [edx], eax        *output = input (store input pointer)
mov eax, 0x1                    return value = 1 (success)
pop ebp
ret
```

This passthrough confirms that cusom does not transform MSG_SPECIAL_BUF
requests. The buffer is routed upstream unchanged. The actual buffer content
is constructed by the caller (qmicro or the SCADA command layer) using the
usodrv::MakeUsoSpecialBuf base class method before passing it to the driver.


### SendZaprosSerNom

Confirmed from disassembly at 0x0804b0c6 in usom2.

Frame construction sequence:

```
byte[ebp-0x10c] = 0x5b    start byte, same as all USOM frames
byte[ebp-0x10b] = 0x5f    command type: serial number query
byte[ebp-0x10a] = tag[0]  device address from the tag struct first byte
byte[ebp-0x109] = 0x00    reserved
```

Send length: 4 bytes (confirmed from the constant 4 passed to SendBuffer).

After SendBuffer, WaitOtvet is called with timeout 0x100 (256) iterations,
giving a 2.56 second maximum wait at 10ms per iteration.

On successful response (WaitOtvet returns non-zero): calls AddOK(edi)
where edi is the device slot index.

On timeout (WaitOtvet returns zero): calls AddError(edi).

The device tag struct is accessed via GetUsomTag rather than GetUsomUSA,
confirming the cusom family uses a different device handle structure from
the altclass/aclass family. The tag struct's first byte at [tag+0x0] is
the device address byte.

The iocuso device array is stored at [esi+0x1c0] (object offset 0x1c0 into
the cusom object), and individual iocuso pointers within that array are
accessed as [array+edi*4] using the device slot index.


### ZaprosSetTi

Confirmed from disassembly at 0x0804b358 in usom2.

Frame construction sequence:

```
byte[ebp-0x124] = 0x5b    start byte
byte[ebp-0x123] = 0x6b    command type: SetTi (writeable teleindication)
byte[ebp-0x122] = tag[0]  device address
byte[ebp-0x121] = 0x00    reserved
byte[ebp-0x120] = 0xff    broadcast or all-points flag
```

A local buffer of 0x18 (24) bytes starting at [ebp-0x24] is zeroed with
memset before filling.

The function iterates up to 8 point slots (loop bound cmp ebx, 0x8). For
each slot at index ebx:

  reads the pending flag at tag[+0x286 + ebx] (byte, checked for non-zero)
  if zero, skips to zero-fill for this slot
  if non-zero:
    calls a virtual function via [esi+0x6c] with three arguments:
      arg0 = the slot count (ebx value)
      arg1 = the word at tag[+0x28e + ebx*2] (the pending TI value for
             this slot, big-endian word)
      arg2 = a pointer to the local result buffer [ebp-0x24]
    checks the result flag at [ebp-0x10] bit 0
    if set, reads the result word at [ebp-0x1c] as edx
    if clear, uses edx = 0

  stores the result big-endian into the outbound frame:
    byte[edi + ebx*2 + 0x5] = (edx >> 8) & 0xff    high byte
    byte[edi + ebx*2 + 0x6] = edx & 0xff             low byte

Send length: 0x15 (21) bytes (header + 8 slots * 2 bytes = 4 + 16 = 20,
plus the 0xff broadcast byte at offset 4 = 21 total).

Waits with timeout 0x100 iterations. Calls AddOK or AddError based on result.


### ZaprosTestTs

Confirmed from disassembly at 0x0804b7a6 in usom2.

Frame construction sequence:

```
byte[ebp-0x10c] = 0x5b    start byte
byte[ebp-0x10b] = 0x63    command type: TestTs (test telestatus)
byte[ebp-0x10a] = tag[0]  device address from tag struct
byte[ebp-0x109] = 0x01    test mode flag (1 = active test)
byte[ebp-0x108] = 0x01    activation flag
```

Send length: 5 bytes.

Waits with timeout 0x100 iterations. The response is parsed by
RaspakTestDiscret.


### RaspakTestDiscret

Confirmed from disassembly at 0x0804969e in usom2.

After calling TestPriem:

Type byte check: buf[1] is compared against 0x63 (the TestTs command
echo). If it does not match, calls AddError and returns 0.

Device address check: buf[2] is compared against tag[0]. Mismatch calls
AddError and returns 0.

Activation flag check: buf[4] is compared against 0x01 (active test mode).
If not 0x01, calls AddError and returns 0.

Data extraction (if all checks pass):

The payload starts at buf[5]. The first byte at buf[5] is the count of
data pairs to follow.

For each data pair at index edi from 0 to count-1:

  reads two bytes at: buf[5 + edi*2 + 1] and buf[5 + edi*2 + 2]
  offset 0: main test value byte stored to tag[+0x204 + edi]
  offset 1: secondary status byte stored to tag[+0x20c + edi]

The offsets 0x204 and 0x20c within the tag struct are distinct from the
USOTM tag offsets, confirming the USOM tag layout differs from the USOTM
tag layout.

On completion, calls AddOK.


### SetGroupTu in usom2

Confirmed from disassembly at 0x0804b16c in usom2.

Frame construction:

```
byte[ebp-0x10c] = 0x5b    start byte
byte[ebp-0x10b] = 0x4c    command type: SetGroupTu (USOM group TU)
```

The device address byte comes from tag[0] at [eax+0x0].

Additional frame fields read from the tag struct at device-specific offsets:
  tag[+0x27f] (byte): group TU bitmask low byte, stored at frame[+0x4]
  tag[+0x27e] (word): group TU word, low byte stored at frame[+0x5]
  tag[+0x27d] (byte): stored at frame[+0x6]
  tag[+0x27c] (word): low byte stored at frame[+0x7]

These tag offsets (0x27c through 0x27f) are significantly higher than the
USOTM SetGroupTu offsets, confirming the USOM tag struct is larger and
differently organized than the USOTM tag struct.


### Working Loop Additions in usom2

Confirmed from disassembly at 0x0804bd38.

The Working loop in usom2 adds two conditional calls not present in usom:

ZaprosSetTi call at 0x804bf0d:
  checks ds:0x804df50 against 0 (a global enable flag for SetTi mode)
  if non-zero, reads the current device's iocuso pointer
  checks [iocuso+0x28] (a count value) against 0 with jle (signed)
  if count > 0, calls GetUsomTag and checks tag[+0x1] against 0x01
  if tag flag is set, checks two local stack variables against 0
  if both conditions met, calls ZaprosSetTi(esi, edi)

ZaprosTestTs call at 0x804bf4f:
  gated by FL_Test global at 0x0804df50
  called with the current device slot index as argument

These additions require close(fd) and InitPort() to be called in the
cleanup path when the error counter threshold at [esi+0x514c] exceeds
NumUso()*2 or when it exceeds 0x14 (20). This port reset pattern is absent
from usom, which relies on the polling cycle itself to recover from errors.

The counter at [esi+0x514c] (object offset 0x514c into the cusom object)
is a 16-bit saturation counter reset to 0 when it exceeds the threshold,
triggering a port close and reinitialisation via InitPort.


## Frame Type Byte Summary

Confirmed type bytes for the USOM protocol:

```
0x57   discrete telestatus response (ZaprosTs -> RaspakDiscret)
0x56   impulse counter response (ZaprosImpuls -> RaspakImpuls)
       these match the USOTM type bytes, suggesting protocol heritage
0x5b   ZaprosSerNom (serial number query) outbound command byte
       note: 0x5b is also the frame start byte in USOTM; the context
       here is as a command type byte at frame offset 1
0x6b   ZaprosSetTi (write teleindication) outbound command byte
0x63   ZaprosTestTs (test telestatus) outbound command byte, also
       the expected echo type in RaspakTestDiscret responses
```

The overlap between 0x57 and 0x56 in both USOTM and USOM protocols
suggests a common heritage or deliberate compatibility at the type byte
level. Whether the framing (CRC algorithm, start byte, length formula)
is also shared requires further disassembly of the USOM TestPriem and
WaitOtvet functions.


## Global State Variables Unique to usom2

```
FL_Test (0x0804df50)
  global flag enabling test mode operations
  when non-zero, ZaprosTestTs is included in the Working poll cycle
  when zero, ZaprosSetTi is also skipped (both are gated by this flag)

Pause (0x0804df4c)
  inter-poll pause in milliseconds
  present in usom2 but absent from usom
  usom uses a hardcoded delay value in the Working loop
  usom2 reads Pause from the RaspakKeys configuration at startup,
  allowing the pause to be set via the Uso= line in start.ini
```

Both usom and usom2 share the Port (0x0804d9c0 in usom2, corresponding
address in usom) and Speed (0x0804ddc4 in usom2) globals for serial port
configuration, confirming both variants read their port and baud rate from
the command line arguments parsed by RaspakKeys at startup.
