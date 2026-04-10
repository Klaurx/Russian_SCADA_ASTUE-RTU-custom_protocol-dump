# aclass Supervisor Analysis (progr/Aqalpha)

## Binary Identity

Binary name: Aqalpha. Class name: aclass. Source file: Aqalpha.cc.
Binary size: 181440 bytes, the largest binary in the firmware set.

Version strings recovered:
  "Aqalpha ver 3.7 arm"
  "[VERINFO] Aqlpha ARM ver 3.6"

The second version string contains a typographical error in the original
firmware source: Aqlpha instead of Aqalpha. This typo appears to have been
present since the original authoring of the version reporting function and
was never corrected across at least the 3.6 and 3.7 firmware releases.

Launch format string: "Aqalpha ver 3.7 arm p/dev/ser[N] s[speed] d[d] ...."
confirming Aqalpha accepts command line arguments for serial port path,
baud rate, and a mode discriminator byte.

Build date: December 9, 2024, consistent with the main firmware release.


## Class Hierarchy

The aclass constructor takes SUPPORTED_USO_AND_BUF_TYPES rather than the
unsigned short used by all other usodrv subclasses. This is the same struct
used by the cusom constructor variants, but aclass is not in the cusom or
cusotm hierarchy. The SUPPORTED_USO_AND_BUF_TYPES parameter encodes a
capability bitmap that allows the supervisor to negotiate supported data types
with the underlying protocol drivers.

The aclass typeinfo symbols confirmed from nm:
  0x0806f444   typeinfo for aclass
  0x0806f43c   typeinfo name for aclass
  0x0806f3e0   vtable for aclass

The vtable at 0x0806f3e0 is the largest vtable in the firmware set,
reflecting the supervisor-level role of aclass which must implement every
virtual method from both the usodrv and kanaldrv hierarchies.


## Network Connectivity

Aqalpha links against libsocket.so.2 and uses the following socket functions:
accept, bind, listen, recv, send, setsockopt, shutdown, socket, inet_addr.

This confirms Aqalpha hosts a TCP server internally in addition to acting
as the RTU supervisor. The TCP server is separate from the upstream RTOS
channel handled by libkanaldrv.so.1. The server likely provides a web
interface or a proprietary management protocol for local administration.

The Apache web server started by restore.scr at /flashdisk/apache serves
the external web interface. Aqalpha's internal TCP server is a separate
management endpoint, possibly for direct supervisory access without going
through the full SCADA channel.

Aqalpha also uses ConnectAttach, ChannelCreate (confirmed from the import
table), MsgSend, MsgReceive, and MsgReply for QNX IPC to coordinate with
qmicro and the field bus drivers.


## Method Table

All methods confirmed from nm with C++ demangling:

```
aclass::aclass(SUPPORTED_USO_AND_BUF_TYPES*)
  constructor, two variants (C1 complete and C2 base object)
  the SUPPORTED_USO_AND_BUF_TYPES argument encodes the capability bitmap
  for the device types this supervisor supports

aclass::~aclass()
  destructor, three variants

aclass::AddUserDataAnalog(iocuso*, iocAnalog*, DEF_ANALOG*, SOST_ANALOG*,
                           unsigned char*, long)
  registers an analog measurement point at address 0x080567fc

aclass::AddUserDataUso(iocuso*, DEF_USO*, SOST_USO*, unsigned char*, int)
  registers a USO device slot at address 0x0806c326

aclass::fdost(unsigned short, MSG_RETURN_ANALOG*)
  analog data fetch for a single channel by index
  at address 0x0804a11c
  takes a channel index and a pointer to the return structure
  reads the scaled analog value for the specified channel and populates
  the MSG_RETURN_ANALOG struct with the engineering unit value

aclass::fener(unsigned short, unsigned int, double)
  energy calculation function at address 0x0804cee6
  takes a channel index as unsigned short, an unsigned int accumulator
  or counter value, and a double precision engineering coefficient
  computes energy in engineering units from raw counter data, applying
  the coefficient to produce watt-hours or reactive power units

aclass::fjur(char*)
  journal output function at address 0x080641f6
  takes a file path string for the journal output destination
  writes accumulated device event data to the specified file path
  likely writes the Aqalpha-level event journal distinct from the
  libjevent.so operational journal

aclass::fkrt(USA*)
  device characterization function at address 0x08056694
  takes a direct USA struct pointer rather than going through GetUsomUSA
  performs device model identification and sets internal capability flags
  based on the model code stored in the USA struct

aclass::fsost(unsigned short)
  device state query by channel index at address 0x08068342
  returns the communication availability state for the specified channel

aclass::fsq(TIME_SERVER_KANAL)
  timestamp sequencing function at address 0x0805675c
  takes a full 6-byte BCD timestamp struct by value (not by pointer)
  manages the timestamp sequence for change detection across poll cycles

aclass::fzUso()
  USO device zero-reset or initialization at address 0x0806840e

aclass::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
  analog value retrieval at address 0x08068d7a

aclass::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
  discrete state retrieval at address 0x08068804

aclass::GetUsomUSA(iocuso*)
  device state resolver at address 0x0804a110
  same single-dereference pattern as altclass::GetUsomUSA:
  returns [iocuso+0x20] as the USA struct pointer

aclass::KorAdr()
  address correction and timing synchronization at address 0x08068616
  same role as altclass::KorAdr: synchronizes device address and timing
  before each poll cycle, called at the start of GetDiscret and GetAnalog

aclass::MakeUsoSpecialBuf(MSG_SPECIAL_BUF*, MSG_SPECIAL_BUF**)
  special buffer routing at address 0x08062a34
```


## Crypto Suite

The crypto primitives resident in Aqalpha are confirmed from nm:

```
0x08050105   des(unsigned char*, unsigned char*, int)
0x0804bd9d   encrypt17(char*, char*, char*)
0x08060a75   encrypt(unsigned char*, unsigned char*)
0x0804abbe   fencrypt(unsigned char*, unsigned char*)
0x08050096   Permutation(unsigned char*, unsigned char*, unsigned char,
             unsigned char*)
0x0804a2f6   SBoxes(unsigned char*, unsigned char*, unsigned char*)
0x0804a27a   Xor(unsigned char*, unsigned char*, unsigned char)
0x08049e4b   (note: bytestobit and bitstobytes are not confirmed by nm;
             they may be inlined or present under different names)
0x080785a0   keys (DES expanded key schedule, data section)
```

The des, Permutation, SBoxes, and Xor functions follow the same signatures
as in qalfat (altclass). The key schedule at 0x080785a0 is at a much higher
address than the qalfat key schedule at 0x8054880, consistent with Aqalpha's
much larger binary size.

The encrypt function at 0x08060a75 takes two unsigned char pointers (the same
signature as qalfat::encrypt), confirming it wraps the DES cipher for frame-
level encryption in the same pattern.

The fencrypt function at 0x0804abbe also takes two unsigned char pointers
and is a second wrapper around the cipher, possibly applying a different
padding or mode than encrypt.

The encrypt17 function at 0x0804bd9d takes three char pointers. This
signature is unique to Aqalpha and is not present in any other binary in the
firmware set. The three-pointer signature suggests: plaintext input, key
input, and ciphertext output as separate buffers (unlike the in-place
operation in qalfat::encrypt). The "17" in the name may indicate 17 DES
rounds (one more than standard 16-round DES), or it may indicate 17-byte
blocks (136 bits, an unusual block size), or it may be a build-time version
discriminator in the function name. Full disassembly is required to
distinguish between these interpretations.


## Device Type Registry

The strings section of Aqalpha contains a set of device model codes that
serve as the supervisor's device type registry. Each code identifies a
specific substation relay protection, measurement, or automation device
model supported by the RTU class polling protocol family.

The format is a 6-digit base number representing the device family followed
by a 2-hex-digit variant code in parentheses. All confirmed device codes:

```
500001(06)    device family 500001, variant 06
507001(01)    device family 507001, variant 01
508001(01)    device family 508001, variant 01
543001(02)    device family 543001, variant 02
543001(03)    device family 543001, variant 03
5500          device family 5500, no variant (abbreviated form)
551001(       device family 551001, variant code truncated in strings output
551001(04)    device family 551001, variant 04
600001(04)    device family 600001, variant 04
601001(06)    device family 601001, variant 06
605001(       device family 605001, variant truncated
605001(06)    device family 605001, variant 06
606001(01)    device family 606001, variant 01
606001(02)    device family 606001, variant 02
606001(2A)    device family 606001, variant 2A (hex variant)
614001(07)    device family 614001, variant 07
616001(06)    device family 616001, variant 06
616001(08)    device family 616001, variant 08
667001(10)    device family 667001, variant 10
680001(0E)    device family 680001, variant 0E
691001(0E)    device family 691001, variant 0E
692001(0E)    device family 692001, variant 0E
693001(11)    device family 693001, variant 11
694001(0E)    device family 694001, variant 0E
695001(0E)    device family 695001, variant 0E
777001(02)    device family 777001, variant 02
778001(01)    device family 778001, variant 01
798001(10)    device family 798001, variant 10
861001(       device family 861001, variant truncated
861001(07)    device family 861001, variant 07
862001(       device family 862001, variant truncated
862001(0000)  device family 862001, 4-digit variant 0000
862001(02)    device family 862001, variant 02
863001(04)    device family 863001, variant 04
863001(10)    device family 863001, variant 10
878001(03)    device family 878001, variant 03
```

These codes identify Russian relay protection and metering device models
from manufacturers such as EKRA, Sirius, RPA, BEPO, and related substation
automation equipment suppliers. The 5-series codes (500001 through 551001)
likely correspond to distance and overcurrent protection relays. The 6-series
codes (600001 through 695001) likely correspond to differential protection
and busbar protection relays. The 7-series (777001, 778001, 798001) may
correspond to specialized protection or communications devices. The 8-series
(861001 through 878001) likely correspond to measurement and metering devices.

The presence of these codes in the supervisor binary rather than in individual
driver binaries confirms that Aqalpha maintains a central device type registry
that maps device model codes to protocol variants and capability sets at
runtime. When a new device is detected during the initial handshake, the
supervisor looks up its model code in this registry to select the appropriate
polling parameters, FCRC variant, and frame format.


## Additional Notable Globals

The named globals in Aqalpha's data segment beyond the device code registry:

```
AK           array key or session acknowledgment key
AkomBuf      accumulator buffer for multi-frame data aggregation
ASKUE        automated commercial accounting of electrical energy
             this is the Russian ASKUE billing metering function (АСКУЭ)
             its presence indicates Aqalpha supports commercial energy
             accounting in addition to protection relay monitoring
ATip         analog type discriminator byte
awt          active wait timer for the primary timing path
awt0         active wait timer initial value
awtt         active wait timer for the secondary timing path
BFER         buffer full or framing error flag
bufsh        buffer sharing state (device availability struct)
CRC          running CRC accumulator
Cust         customer or device identifier
D            primary data buffer
dalp         delta alpha, deadband value for analog change detection
delkor       correction delta for address correction
delkoris     historical correction delta (for drift compensation)
dkon         end-of-session time marker
dkrf         device-specific scaling coefficient table
dkor         current address correction value
dl           data length counter
dlt          delta time between consecutive poll cycles
dost         device availability flag (dostupnost, Russian for availability)
dPor         port delta (difference between expected and actual port timing)
DST          daylight saving time flag
DSTS         DST start time
DSTW         DST week configuration
dvp          voltage phase A measurement
dvpa         voltage phase A (alternative or auxiliary measurement)
ER           error register
```

The ASKUE global is the most significant unique feature of Aqalpha compared
to the other RTU class drivers. АСКУЭ (Automated System for Commercial
Accounting of Electrical Energy) is a Russian regulatory requirement for
substations connected to the grid. Its presence in the supervisor confirms
that Aqalpha implements commercial metering data aggregation and reporting
in addition to the telemetry functions shared with the other drivers.

The ConvLocTimeSyst, ConvLocToSer, and ConvLocToSerKan functions in Aqalpha
confirm it performs BCD timestamp conversion for all data types, consistent
with the supervisor needing to timestamp every event and measurement from
all connected devices regardless of protocol family.

NEXT_PRIBOR and setimptimer confirm the supervisor pattern: NEXT_PRIBOR
advances the poll sequence to the next device, and setimptimer initializes
the impulse counter accumulation timing.

The facp function (at 0x0804bef0, signature facp(unsigned char*, unsigned char))
and fadr (at 0x0804d0d7, signature fadr(char*)) are address processing
functions. fakn (at 0x08052cde) takes a single unsigned char and is likely
a frame acknowledgment or numbering function. fawti1 (at 0x08050005) takes
a short and is a wait-with-timeout function.

The digff function (at 0x0804ac8e, signature digff(unsigned char)) appears
in Aqalpha, qpuso, qmir, qpty, and qptym but not in qalfat. It processes
a single byte and likely extracts digit fields from BCD-encoded values,
consistent with a BCD digit extraction function.
