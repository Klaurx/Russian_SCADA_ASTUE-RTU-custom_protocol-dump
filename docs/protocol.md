# Protocol Wire Format

## RTOS Link Layer (libkanaldrv.so.1)

### Frame Types

The RTOS protocol operates over TCP (primary), UDP (secondary), and VHF
packet radio (tertiary backup). Two categories of frames exist at the link
layer: the srez (snapshot) carrying bulk I/O data, and the kvitok (ACK/NACK)
used for session acknowledgment.

### Srez (Snapshot) Frame

Confirmed from disassembly of MakeSrezBuf() and MakeSrezWithFictiveTime().
The first 10 bytes are always zeroed then populated as follows before payload
is appended.

```
Offset   Size   Meaning
0        1      Magic byte, always 0x75 (ASCII 'u', from USO)
1        6      Timestamp, 6 bytes BCD encoded YY MM DD HH MM SS
7        1      Frame type discriminator
8        2      Reserved, always 0x0000
```

Frame type discriminator values recovered from MakeSrezBuf():

```
0x82   Fictive time buffer (srez with synthesised timestamp)
0x7F   Normal data buffer
0x01   Discrete type sub-frame
0x02   Analog type sub-frame
0x09   Impulse/counter sub-frame
```

The SOST_SEND struct field at offset +0x2 holds the item count for the
payload. Items are laid out contiguously with fixed strides by type.

Item strides confirmed from MakeSrezBuf() loop analysis:

```
Discrete (DI)   0x1C bytes per item (28)    status flag at [+0x1B] bit 1
Analog (AI)     0x68 bytes per item (104)   status flag at [+0x61] bit 2
Impulse/counter 0x10 bytes per item (16)    status flag at [+0x0C] bit 0
```

### Address Frame (MakeAdrBuf)

Confirmed from disassembly. Identical 10 byte header as srez. At offset 7 the
type byte is computed as:

```
if param == 1:  byte[7] = 0x75 - 0x73 - carry = 0x82 (with SBB trick)
if param == 0:  byte[7] = 0x7F
```

The vtable call at [eax+0x98] inside MakeAdrBuf fills bytes 1 through 6 with
the timestamp from the GPS shared memory object.

### Kvitok (ACK) Frame

SendKvitok signature: (SOST_PRIEM*, SOST_SEND*, buf1*, buf2*, uint16,
RTOS_RETRANSLATE_ADR*, uint16). The exact byte layout of the kvitok is not
yet fully disassembled. Known from context: it is a short fixed-length frame
carrying the sequence number from SOST_SEND[+0x2] and the channel address
from RTOS_RETRANSLATE_ADR.

### Session and Timing

The Obmen() main loop (offset 0x5652 in libkanaldrv.so.1) implements:

```
1   If periodic timer fired: lock buffer, call send function for buf1 or buf2
2   If idle counter exceeds 10: increment error counter at [+0x97C0]
3   atomic_set(0xABCD) on RX flag
4   Check session timeout [+0x228], call TimeReInitKanal() if exceeded
5   ReadByteFromPort() one byte at a time
6   On success, call PriemPacket() then AnalizBufPriem()
7   Switch on AnalizBufPriem return code:
      0x1   Full response dispatch via AnalizBufPriemAndSendOtvet()
      0x2   Send kvitok if ack_mode flag [+0x226] is set
      0x4   Partial frame, continue accumulating
      other Close session (CloseSeans via vtable[+0x2C])
```

### Command Dispatch Table

AnalizBufPriemAndSendOtvet() handles 14 command codes 0x0 through 0xD via a
jump table at offset 0x4B22.

```
Code   Handler behaviour
0x0    Send buffer 1 via SendBuffer
0x1    Async poll, call MakeNextSrezBuf and send
0x2    Snapshot query, check TX ring buffer index [+0x9754]
0x3    Set new value, acquire LockBuffer(4), call AnalizBufPriem indirectly
0x4    Init/handshake, unpack RTOS_RETRANSLATE_ADR into [+0x17004..0x1700E]
0x5    Request with flag=1 then SendBuffer
0x6    Send buffer 0 or request with flag=0
0x7    Multi-item request, iterate over item count
0x8    Analog read, call vtable[+0x178]
0x9    Discrete read type A (count tag from [+0x21D]), call vtable[+0x17C]
0xA    Discrete read type B (count tag from [+0x21E]), call vtable[+0x180]
0xB    Secondary discrete read, call vtable[+0x184]
0xC    Reboot sequence: flushall, sleep(5), flushall, sleep(6), sysmgr_reboot
0xD    Unknown command, call MakeSoob error handler
```

Command 0x4 (handshake) extracts from RTOS_RETRANSLATE_ADR and SOST_SEND:

```
[obj+0x17004] = RTOS_RETRANSLATE_ADR[+0x8]   primary address
[obj+0x17006] = RTOS_RETRANSLATE_ADR[+0x0]   channel ID
[obj+0x17008] = SOST_SEND[+0x2]               sequence number 1
[obj+0x1700A] = RTOS_RETRANSLATE_ADR[+0xC]   tertiary address
[obj+0x1700C] = RTOS_RETRANSLATE_ADR[+0x4]   secondary address
[obj+0x1700E] = SOST_SEND[+0x2]               sequence number 2
```

### Shared Memory Naming Scheme

All IPC buffers use QNX POSIX shared memory with names formatted as:

```
/Sem%.04u     semaphore object
/SB%.04u      send buffer
/BE%.04u      buffer event
/BP%.04u      buffer packet
Nreg%.04u.bin register file (persistent)
Per%.04u.bin  period file (persistent)
```

The unit number filling the %04u slot comes from the 'a' command line
parameter (unit_id at object offset +0x4).

### Transport Parameters (start.ini Kanal= line)

```
Kanal=tcpqnx  a<unit_id>  i<localIP>,<localPort>,<remoteIP>,<remotePort>

a   unit address (uint16, stored at obj+0x4, initialised 0xFFFF)
i   IP/port config parsed by ctcpqnx::RaspakKeys()
    localIP bound by the RTU
    localPort the RTU listens or sends from
    remoteIP the SCADA master
    remotePort the SCADA master port

Example from deployment (addresses redacted):
  Kanal=tcpqnx  a111  i[REDACTED],2124,[REDACTED],5124
```

The asymmetric ports confirm the master has a fixed well-known port (5124)
while the RTU uses 2124 as its identity port.

---

## USOTM Field Bus Protocol (usotm binary, cusotm class)

### Physical Layer

Serial port (QNX /dev/serN), default /dev/ser7 or /dev/ser9 depending on
config. Baud rate set by the Speed command line parameter, defaulting to a
value stored in the global ds:0x804EEC4 and applied via cfsetispeed and
cfsetospeed. VMIN=0, VTIME=0 (from termios analysis of InitPort).

The port file descriptor is stored at cusotm object offset +0x4E18. All reads
go through a single byte read() loop in WaitOtvet.

### WaitOtvet() Receive Loop

Confirmed from disassembly at 0x804BC60. The algorithm is:

```
timeout_counter = 0
max_timeout = 0x64 (100 iterations at 10ms = 1 second initial wait)

loop:
  read(fd, buf+byte_count, 1)
  if read returns <= 0:
    delay(10ms)
    timeout_counter++
    if timeout_counter >= max_timeout: return byte_count (timeout)
  else:
    if byte_count == 0:
      call IsFirstByte(byte)
      if not valid start: skip byte and continue
    byte_count++
    if byte_count >= requested_len: return byte_count
    max_timeout = 2  (tight 20ms window between subsequent bytes)
    timeout_counter = 1
```

If the overall receive times out with byte_count == 0, the error counter at
object offset +0x5084 is incremented. If timeout occurs mid-frame (byte_count
> 0), that counter is reset to 0.

### TestPriem() Frame Validation

Confirmed from disassembly at 0x804971A. Algorithm:

```
if len <= 5: return invalid (minimum frame is 6 bytes)

checksum = 0

for i from 1 to len-3 (inclusive):
  checksum = accumulate(checksum, buf[i])
  (accumulate is a vtable call at [ecx+0x11C])

expected_hi = (checksum >> 8) & 0xFF
expected_lo = checksum & 0xFF

if buf[len-2] != expected_hi: return invalid
if buf[len-1] != expected_lo: return invalid

payload_count = buf[3]
if (payload_count + 6) != len: return invalid

return valid
```

Frame layout implied by the length check:

```
Offset   Size   Field
0        1      Start byte (validated by IsFirstByte)
1        1      Response type byte
2        1      Device address (compared against tag at [+0x0F])
3        1      Payload item count (N)
4        N*?    Payload items (type dependent)
len-2    1      CRC high byte
len-1    1      CRC low byte
```

Total frame length = N + 6 (header=4 + CRC=2).

### Response Type Bytes

Recovered from type checks in RaspakDiscret, RaspakAddAnalog, RaspakImpuls,
RaspakTempVn:

```
0x57   Discrete poll response
0x56   Impulse counter response
0x5D   Internal temperature (TempVn) response
0x86   Analog values response
```

### Discrete Frame Parsing (RaspakDiscret)

Confirmed from disassembly at 0x804A38C.

After TestPriem passes and type byte 0x57 is confirmed, address at buf[2] is
checked against the device tag field at [tag+0x0F]. On mismatch: AddError.

The item count is derived from buf[3] using the multiplier 0xAB:

```
count = ((buf[3] * 0xAB) >> 8) >> 2
```

This is integer division by approximately 85/32, effectively dividing by a
scale factor. The result is the number of 16-bit status words in the frame.

Each status word is read from buf at positions [4 + i*2] (high byte) and
[5 + i*2] (low byte), assembled big-endian, and stored into the device tag
structure at computed offsets. The tag structure fields confirmed:

```
[tag + 0xBC]   status flags byte, OR'd with 0x02 (data fresh), 0x10, 0x04
[tag + 0x04 + i*2]   current value word (offset base 0x50 items)
[tag + 0x0C + i*2]   mask word
[tag + 0xC8 + i*2]   previous value word (for change detection)
[tag + 0xB4 + i*2]   received value word
```

After all items processed, AddOK(uso_index) is called.

### Analog Frame Parsing (RaspakAddAnalog)

Confirmed from disassembly at 0x804B0E6.

Type byte must be 0x86. Address checked at buf[2] vs tag[+0x0F]. The item
count is at buf[3] and compared against 0x37 (55 maximum channels). If count
exceeds 55, the frame routes through a thread synchronisation path using a
global mutex at 0x804EEC8 and condition variable at 0x804EED0. This indicates
the analog processor runs on a separate thread and RaspakAddAnalog signals it.

The analog payload is moved into the tag structure starting at offset +0x1C6:

```
memmove(tag + 0x1C6, buf + 4, count)
```

The count is then stored at tag[+0x1FE] as a uint16.

After data transfer, the producer signals the consumer thread via
pthread_cond_signal and the consumer resets [edi+0x4E24] to 0xFFFFFFFF when
done.

### Impulse Counter Frame Parsing (RaspakImpuls)

Confirmed from disassembly at 0x804B2AE.

Type byte must be 0x56. Up to 32 counters (loop 0 to 31, cmp ebx,0x20).

The frame carries 4 bitmask bytes at offsets 4,5,6,7. For each of the 32 bit
positions, if the bit is set in the bitmask, a 16-bit counter value is read
from the variable-position payload starting at buf[8] (base offset 0x8,
advancing by 2 per active counter).

For each active counter, the 16-bit value is compared to the previous stored
value at tag[+0xEC + counter*4]:

```
if new == prev:   no change
if new >= prev:   delta = new - prev, added to accumulator at tag[+0x16C + counter*2]
if new < prev and prev > 0x7FFF:
    delta = (NOT prev + 1) + new (handles 16-bit rollover)
    added to tag[+0x16C + counter*2]
if new < prev and prev <= 0x7FFF:
    delta computed via [tag + 0xB0 + counter*2 + 0xC] table
```

The overflow detection threshold is 0x7FFF, meaning counters are treated as
unsigned 16-bit with rollover detection at midpoint.

Change flags are maintained at tag[+0xE4 + byte] and tag[+0xE8 + byte] as
bitmasks.

### Internal Temperature Frame (RaspakTempVn)

Confirmed from disassembly at 0x8049E54.

Type byte must be 0x5D. Frame length must be exactly 8 bytes (check at
si==0x8). Temperature value is at buf[4] (high byte) and buf[5] (low byte),
assembled as a signed 16-bit integer and stored at tag[+0x1B4] as int32.
A flag byte at tag[+0x1BE] is set to 1 indicating fresh data.

### AddKod() Command Encoding

Confirmed from disassembly at 0x804AAD8.

AddKod(uso_index, point_number, value) is the USOTM outbound command builder.
It acquires the global mutex, signals the transmit thread if needed, then
encodes the command into the device tag structure:

```
bit_position = point_number % 8
byte_position = point_number / 8
tag[+0x17 + byte_position] |= (1 << bit_position)   command pending bitmask
tag[+0x1C + point_number*4] = value                  command value
```

GetLocalTime is called to timestamp the command at tag[+0x9C].

If point_number is negative (sign bit set), AddKod returns immediately without
encoding, used as a no-op sentinel.

### SendTu() Telecontrol Command

Confirmed from disassembly at 0x804C18A.

SendTu(uso_index) looks up the device tag, checks the pending TU flag at
tag[+0x1AE]. If non-zero, calls SetGroupTu(uso_index) which builds an 18-byte
outbound control frame:

```
Offset   Value    Description
0        0x5B     Start byte
1        0x04     Command type (telecontrol group)
2        tag[+0x39]  Device address byte
3        0x00
4..7     4 bytes from tag[+0x4]   Device identification block
8        0x00
9        0x00
10..13   4 bytes: bitmask word and its bitwise NOT packed together
           low16 = NOT(mask) << 16 | mask
14       0x01 if activate, 0x00 if deactivate
frame sent as 18 bytes (0x12)
```

After sending, WaitOtvet is called with a 500ms timeout (0x1F4 iterations).

### InitPort() Serial Configuration

Confirmed from disassembly at 0x804C9AE.

Opens the port path stored at ds:0x804EAC0 with O_RDWR (flags=2). Reads
current termios, applies the following:

```
c_iflag &= 0xFFFFCA80   (clear most input processing)
c_cflag &= 0xFFFFFAFF + OR 0xB0  (8N1 equivalent, set CS8+CREAD)
c_lflag &= 0xFFFF7C04   (raw mode, no echo)
c_oflag &= 0xFFFF7C04
c_cc[VMIN] = 0
c_cc[VTIME] = 0
```

Baud rate applied via cfsetispeed and cfsetospeed from the global Speed value.

---

## KPRIS Protocol (kpris binary, ckpris class)

### Command Strings Recovered

Full set of operation names recovered from the binary strings section. These
map directly to outbound frame type codes sent via SendBuffer/WaitOtvet:

```
Zapros Ti Kol        Request TI (teleindication) count
Zapros Oscill        Request oscillogram capture
Zapros spisok files  Request file listing
Zapros Read File     Request file read
Zapros Write File    Request file write
Zapros Get Info      Request device identification
Zapros Reboot        Request device reboot
Zapros Delete File   Request file deletion
Zapros Spisok Journals   Request journal listing
Zapros Journal       Request journal download
Zapros current Config    Request running configuration
ZaprosSysInfo        Request system information
Zapros Ti NomUso     Request TI for specific USO node
Zapros Ts NomUso     Request TS for specific USO node
```

Control commands:

```
SetTu1               Group telecontrol command
SetOneDout           Single discrete output command
SetOneTu             Single telecontrol command
```

The KPRIS protocol also carries per-device identification via IDENT structs,
address fields NomUso (device number) and AdrUso (device address as hex), and
a coefficient system (KTI, KTU) for measurement scaling.

Additional parameters recovered from the symbol table:

```
Stopbits    serial stop bits (1 or 2)
Parity      serial parity setting
Bits        serial data bits
Speed       baud rate
Pause       inter-character timeout ms
DefaultIp   fallback IP string for UDP variant
DefaultPort fallback port for UDP variant
TypeUso     device type discriminator
```

The ckpris class maintains its own oscillogram request pipeline
(SendZaprosOscill, MakeZaprosOscill) and a two-level TU queue:
AddTuInQueneLow and AddTuInQueneHigh (priority command queuing).

---

## IEC 60870-5-101 Link Layer (ekra binary, cekra class)

### Confirmed Frame Fields

Recovered from debug format strings in the ekra binary:

Master frame (DIR=1, PRM=1):

```
DIR(n)   direction bit (1 = master to slave)
PRM(n)   primary message bit
FCB(n)   frame count bit (alternates for retry detection)
FCV(n)   frame count valid
fn(n)    function code
ADDR(n)  link address
```

Slave frame (DIR=0, PRM=0):

```
DIR(n)   direction bit
PRM(n)   primary message bit
ACD(n)   access demand (class 1 data available)
DFC(n)   data flow control (buffer full)
fn(n)    function code
ADDR(n)  link address
```

ASDU header fields:

```
TYP(n)         type identification
STR_QUAL(n.n)  structure qualifier (number of objects, SQ bit)
COT(n)         cause of transmission
ADDR(n)        ASDU address (common address)
TFN(n)         time tag (if present)
NINF(n)        number of information objects
```

The one-byte ACK value 0xE5 is confirmed (single character acknowledge, FT1.2
standard).

### Message Queue Names

IEC 101 events are routed via POSIX message queues with names:

```
/ekrauso%d   (one queue per USO device index)
```

Queue operations: iecTimeSync, iecClass1Poll, iecClass2Poll, iecGlobInterr.

---

## Modbus CRC

Both sirius_mb and its NTU variant include a CRC lookup table confirmed by
the symbol names _ZL9auchCRCHi and _ZL9auchCRCLo (static high and low byte
tables). The CRC update function is exported as CRC_Update(uint8_t*, uint16_t)
confirming standard Modbus CRC16 with the 0xA001 polynomial reflected variant.
