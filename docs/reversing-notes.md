# Reversing Notes

## Methodology

All analysis is static. No dynamic instrumentation, no live system access,
no network connections to operational infrastructure of any kind.

Primary tools and techniques:

  strings on all ELF binaries. High yield due to the non-stripped state of
    every binary in the firmware image. Every class name, method name, and
    parameter type is recoverable without disassembly.

  nm with the C++ demangling flag for full class and method signature
    extraction including parameter types and return types.

  objdump with Intel syntax disassembly for targeted function analysis.

  Manual annotation of QNX IPC patterns including MsgSend, MsgReceive,
    MsgReply, ChannelCreate, ConnectAttach, and shared memory patterns.

  Configuration file semantic analysis of start.ini, modem.cfg, restore.scr,
    and binary strings which contain configuration content verbatim.


## Compiler and ABI Notes

All binaries use GCC 4.3.3 targeting i386 QNX Neutrino with position-
independent code (-fPIC). The calling convention is standard i386 cdecl
with the this pointer passed in eax for thiscall variants. Some optimised
functions use esi or edi as the object register instead. C++ name mangling
follows the Itanium ABI which nm with -C demangles cleanly.

Floating point uses the x87 FPU. The fldz, fstp, and fadd patterns are
visible throughout libkanaldrv.so.1, librashet.so, and qalfat where ftek18
reads float scaling coefficients. The stub methods GetStaticTimeSend and
GetDinamicTimeSend in the kanaldrv base class both return 0.0 via fldz
followed by ret, confirming they are base class stubs overridden by transport
subclasses.

Full RTTI is enabled across the firmware. Type information structures and
vtables are fully present and demangleable. Class hierarchies are visible
from the typeinfo and vtable symbols without any disassembly.

The two-variant constructor pattern (complete constructor C1 and base
constructor C2) is present throughout the C++ class hierarchy, confirming
the full Itanium ABI. Destructors appear in D0 (deleting), D1 (complete),
and D2 (base object) variants.


## Binary Inventory and Classification

The firmware contains 78 distinct binaries across 7 directories. The
following classification covers all confirmed binaries.

Core measurement engine:
  qmicro    central measurement and calculation supervisor, QMICRO class
  Aqalpha   primary application supervisor, aclass base, source Aqalpha.cc

Channel drivers (all implement kanaldrv vtable):
  libkanaldrv.so.1  base class and RTOS frame layer
  tcpqnx            ctcpqnx TCP transport, primary WAN path
  tcparm            ctcpqnx TCP alarm transport via libkanalarm.so.1
  sercom            csercom serial transport
  gsmlink           cgsmlink GSM modem transport, subclass of csercom
  udpqnx            cudpqnx UDP transport
  udpretransl       cudpretr UDP retranslation transport
  rs485retransl     crs485retr RS-485 retranslation via libretransldrv.so.1
  rs485dosretransl  crs485retr RS-485 retranslation via libretransldos.so.1
  tnc               ctnc TNC packet radio transport with full AX.25 support
  p104send          cp104send IEC 60870-5-104 upstream server

Retranslation drivers:
  libretransldos.so.1  DOS-mode retranslation, retransldrv class
  libretransldrv.so.1  driver-mode retranslation, retransldrv class

Field bus protocol drivers (all implement usodrv vtable):
  usotm / cusotm      USOTM serial field bus, primary I/O protocol
  usotmj / cusotmj    USOTM with SOE journal extension
  usom / cusom        USOM serial field bus, third distinct USO class
  usom2 / cusom       USOM with serial number query and test mode
  ekra / cekra        IEC 60870-5-101 for EKRA relay protection IEDs
  kpris / ckpris      KPRIS substation automation protocol
  sirius_mb           Modbus RTU for Sirius relay IED family
  sirius_mb_ntu       Modbus RTU NTU variant of above
  mdbf                Modbus RTU over serial
  mdbf80              Modbus RTU 80-series enhanced variant
  mdbfo               Modbus RTU open variant
  mdbtcp              Modbus TCP
  mdbrturetr          Modbus RTU retranslation
  mdbtcpretr          Modbus TCP retranslation
  serpr / cserpr      SERPR srez retranslation protocol
  stem300 / cmdbf     STEM-300 and CMDBF meter protocols
  cicpcon             ICP-CON protocol for ICP-CON field devices
  rpn / crpn          RPN protocol for stepped relay RTU class devices
  comdirect           direct COM port driver, raw serial without framing
  gpio86dx            PC/104 digital I/O board driver, x86 DX variant
  uso_amux (amux)     PC/104 analog multiplexer driver, plain variant
  uso_amux (amux86dx) PC/104 analog multiplexer driver, x86 DX variant
  uso5600 (c5600)     c5600 PC/104 ISA digital I/O board driver
  fixuso              virtual USO in shared memory for testing
  usoimit             USO simulator creating /ConfigImit segments

RTU class polling drivers (all follow the USA/KorAdr/fopimpt pattern):
  qalfat / altclass   DES-encrypted altclass protocol, version 1.8
  qcet / ctclass      class-based RTU polling, version 3.3
  qmir / mirclass     MIR variant RTU polling, version 1.1
  qpuso / pusclass    PUS variant RTU polling, version 1.0
  qpty / pclass       PTY variant RTU polling, version 3.0 ARM
  qptym / ptmclass    PTM variant RTU polling, version 5.0

Support libraries:
  libusodrv.so.1    USO data model base class
  libservdrv.so.1   QNX IPC service driver
  librashet.so      engineering calculation engine
  libjevent.so.1    event journaling module
  libtmkorr.so      GPS time correction library
  libsystypes.so.1  shared type definitions
  libwatch86dx.so   hardware watchdog for x86 DX board
  libwatch586.so    hardware watchdog for 586 class hardware
  libterm.so        terminal abstraction for local HMI panel

Hardware and system utilities:
  rmodem            TNC packet radio resource manager (two variants)
  gpstime           GPS time synchronisation daemon
  infogps           GPS status query utility
  dev-udp           UDP virtual serial port resource manager
  tcpsrv            TCP-to-serial bridge (newest binary, July 2025)
  modem_gsm         GSM modem command utility
  power             GPIO power control utility, PowerON via ThreadCtl
  sqmicro           secondary watchdog and permission fixer
  sram_format       SRAM formatting utility
  devf-sram         QNX flash and SRAM device driver
  flashctl          flash filesystem control utility, standard QNX binary
  pdebug            QNX GDB-protocol remote debugger, standard QNX binary
  restore           restore.scr script executor
  netcfg            network configuration utility
  qconn             QNX connection manager, standard QNX binary
  qpty              PTY multiplexer, standard QNX binary (different from
                    the qpty RTU class polling driver in progr/)


## Complete Findings: altclass Protocol (qalfat)

Full notes are in reversing/notes/altclass.md. Key points:

The altclass binary (qalfat, version 1.8) implements a serial field bus
protocol with DES encryption applied at the frame level. The DES
implementation is resident in the binary itself, not in a shared library.
The crypto primitives are des, Permutation, SBoxes, Xor, bytestobit,
bitstobytes, and encrypt. The encrypt function takes two unsigned char
pointers (plaintext buffer and key buffer) and performs a single DES
encryption operation operating on 64-bit (8-byte) blocks after bit
expansion via bytestobit.

The FCRC18 algorithm used for frame integrity is not a standard CRC. Its
construction: the seed is formed by bitwise NOT of buf[1] shifted into the
high byte ORed with bitwise NOT of buf[0] in the low byte. Bytes 2 through
len-1 are fed through CRC-16/KERMIT (polynomial 0x8408, the reflected form
of 0x1021) one byte at a time. Two zero bytes are then fed through the
accumulator. The final result is bitwise NOTed and byte-swapped. This three-
layer construction is confirmed from disassembly of FCRC18 at 0x8049edf and
the crc16 inner function at 0x8049e9a.

The fopimpt thread implements the complete session state machine. The session
state is stored in the byte at 0x8052ea2. The state machine dispatches via
a jump table at 0x8050ca8 with state values ranging from 0x00 through 0x48.
States below or equal to 0x08 trigger the initial handshake sequence via
fzapr. States in the 0x14 and 0x16 range trigger the main data exchange
sequence. State 0x40 is a special long-timeout state used when the device
count is large.

Encryption is applied to outbound data frames, not to configuration. The
encrypt call at 0x804c814 is preceded by two bytestobit calls that expand
the plaintext block and the key block from byte arrays to per-bit arrays
before calling des with mode=1 (encrypt). The bitstobytes call at 0x804c820
that follows collapses the result back from per-bit form to bytes. This
sequence is part of the fopimpt state machine at the point where an
authenticated data frame is being prepared for transmission.

The fend_send function at 0x804c5b3 appends the FCRC18 checksum to the
outbound frame. When argument is 1 (transmit mode), it checks the type byte
at 0x8052ec0 against 0x6. If the type byte is 0x6, it uses len-1 as the
CRC input length (operating on the sub-frame starting at 0x8052ec1). If the
type byte is not 0x6, it uses the full frame length. After computing FCRC18,
it writes the high byte at buf[len] and the low byte at buf[len+1], then
increments the length counter at 0x8052ea4. When argument is 0, fend_send
skips the CRC computation and goes directly to the send path. The send path
calls write() with file descriptor from 0x8052df8, the buffer at 0x8052ec0,
and the length from 0x8052ea4. After writing, it calls readcond() with a
500ms timeout (0x1f4 = 500 iterations) on the same file descriptor.

The Read_OK181 function at 0x804af85 reads and validates received frames.
It compares the receive position counter KolPriem at 0x8052ea6 against the
base counter KolPr at 0x8052eac. If they are equal, no new data is available
and the function returns 0. If KolPriem equals 1, it checks the first byte
of the receive buffer against type codes 0x15 (session init acknowledgment)
and 0x06 (data acknowledgment). For general frame positions, it checks the
byte at pbuf+KolPr against 0x6 and 0xee (two valid frame delimiter values).
When a valid frame is found, it computes the frame span as KolPriem minus
KolPr. The return values are 0 (no data), 1 (ok, complete frame), 4 (error,
device set to state 4), and 6 (ok with alternate success path).


## Complete Findings: aclass and Aqalpha

Full notes are in reversing/notes/aclass-aqalpha.md.

The Aqalpha binary is the primary application supervisor. Its class is
aclass, which is distinct from all field bus driver classes. The constructor
takes SUPPORTED_USO_AND_BUF_TYPES confirming it operates at supervisor level.

The aclass crypto suite includes des, Permutation, SBoxes, Xor, bytestobit,
bitstobytes, encrypt (same pattern as altclass), and additionally encrypt17
which takes three char pointers and is a distinct 17-round or 17-block cipher
variant not present in qalfat. The fencrypt wrapper (two unsigned char
pointers) applies frame-level encryption to outbound data.

The Aqalpha binary contains the largest collection of device type strings in
the firmware: device codes such as 500001(06), 507001(01), 551001(04),
600001(04), 605001(06), 606001(01), 614001(07), 616001(06), 667001(10),
777001(02), 778001(01), 798001(10), 861001(07), 862001(02), 863001(04),
863001(10), 878001(03). These are substation device model codes identifying
the types of relay protection, measurement, and automation devices supported
by the RTU class polling protocol family. The format is a 6-digit base number
followed by a 2-digit variant in parentheses.

Aqalpha's data segment contains a large number of named globals reflecting
the breadth of the protocol. Among the notable ones: AK (array key or
acknowledgment key used in session management), AkomBuf (accumulator buffer),
ASKUE (automated commercial accounting of electrical energy, the Russian
abbreviation for the billing metering function), ATip (analog type byte),
awt, awt0, awtt (active wait timers for multiple timing paths), BFER (buffer
error flag), CRC (running CRC accumulator), Cust (customer or device
identifier), D (data buffer), dalp (delta alpha, deadband value for analog
change detection), delkor (correction delta), delkoris (historical correction
delta), dkon (end time marker), dkrf (coefficient table for device-specific
scaling), dkor (correction value), dl (data length), dlt (delta time), dost
(availability flag), dPor (port delta), DST (daylight saving time flag), DSTS
(DST start), DSTW (DST week), dvp and dvpa (two-phase voltage values), ER
(error register).


## Complete Findings: usotmj

Full notes are in reversing/notes/usotmj.md.

The cusotmj class extends cusotm with SOE (sequence of events) journaling.
Change-driven polling replaces cyclic polling for telestatus and
teleindication. The TypeUsoJournal global discriminates journal-capable
device types from standard types at runtime.

The SendZaprosDiscret signature in cusotmj takes an additional int parameter
compared to the base cusotm version. This extra parameter controls whether
the request is for full state synchronisation or incremental change reporting.
The two binaries are not drop-in replacements for each other.

The __SS__ global unique to cusotmj tracks session state across multiple
change-driven poll cycles, enabling the journal to correlate events across
poll boundaries.


## Complete Findings: cusom and cusom2

Full notes are in reversing/notes/cusom-variants.md.

cusom implements a third distinct USO field bus class, separate from cusotm.
usom2 adds serial number query (SendZaprosSerNom), test loopback mode
(ZaprosTestTs, RaspakTestDiscret), writable teleindication (ZaprosSetTi),
the FL_Test mode flag, and the Pause inter-poll delay global.

The SetGroupTu method in usom2 reads from tag offsets at +0x27c through
+0x27f for its command parameters, which are distinct from the USOTM
SetGroupTu offsets, confirming a different tag layout for the USOM field bus.

The MSG_SPECIAL_BUF routing through MakeUsoSpecialBufZaprosUso is a three-
instruction passthrough that writes the input pointer to the output pointer
and returns 1, confirmed from disassembly of 0x8049b12 in usom2.


## Complete Findings: cp104send

Full notes are in reversing/notes/cp104send.md.

The cp104send class is a complete IEC 60870-5-104 server implementation
subclassing kanaldrv. It handles all three APDu frame types (I, S, U) and
implements the full ASDU construction for spontaneous data, general
interrogation, time synchronisation, and telecontrol. The srez-to-IEC-104
conversion path CreateSrezForRemoteKp converts the RTOS internal snapshot
format into a sequence of IEC 104 ASDUs for transmission to upstream SCADA
masters over TCP.


## Complete Findings: Transport Layer Drivers

Full notes are in reversing/notes/transport-layer.md.

The tcparm binary uses libkanalarm.so.1 rather than libkanaldrv.so.1,
confirming it is a distinct alarm channel variant of the ctcpqnx TCP driver
with different timing semantics.

The gsmlink binary (source: qnxgprs.cc, not gsmlink.cc) implements
cgsmlink subclassing csercom. This binary is the GPRS-specific TCP transport
driver, distinct from the gsmlink AT command utility in the same directory.
The source file name qnxgprs.cc in the strings section confirms the binary
was compiled from the GPRS-specific source rather than the generic GSM source.

The ctnc class implements full AX.25 packet radio with the CheckCTS and
CheckDSR hardware handshake checks, the InitTnc AT command initialization
sequence, and the OprosAllChannels multi-channel polling loop. The TNC
command strings JHOST1, P255, QRES, and the per-message format strings
"%ld %ld %ld %ld %ld %ld %ld" and "@T2%ld" are TNC2-style terminal node
controller commands.

The two crs485retr variants (rs485retransl and rs485dosretransl) are
functionally identical in the crs485retr class but link against different
retranslation libraries, allowing the same RS-485 hardware path to operate
in either driver mode or DOS compatibility mode.


## Complete Findings: Hardware Drivers

Full notes are in reversing/notes/hardware-drivers.md.

The c5600 binary implements uso5600 for the c5600 PC/104 ISA board. It
uses atomic_add and atomic_sub (QNX atomic operations) and configures
real-time thread scheduling explicitly via pthread_attr_setinheritsched and
pthread_attr_setschedparam. The ConnectToDiscrets and ConnectToImpuls calls
from usodrv confirm it writes directly to the /SostDiscrets and /SostImpulses
shared memory segments without any serial protocol intermediary.

The amux86dx binary adds SetAdr(unsigned char) to the uso_amux interface,
confirming the x86 DX variant uses an addressable multiplexer where channel
selection is explicit. The plain amux variant reads all channels without
explicit addressing.

The comdirect class uses a Windows-style backslash path "\dev\ser1" which
is a distinct anomaly compared to the forward-slash paths in all other
drivers, suggesting this driver originated in a non-QNX codebase.


## Complete Findings: Support Libraries

Full notes are in reversing/notes/support-libraries.md.

librashet.so has 157 exported symbols confirmed from nm. The internal label
symbols from .L442 through .L756 (spanning over 300 labels) indicate that
the formula evaluation and group analysis functions (RashetAnalog, RashetDiscret,
GroupAnaliz, GroupAnaliz1, GroupAnaliz2, TestRash, TestRash1) are very large
functions with extensive branching. The _SetAnalogValue, _SetConstantValue,
_SetDiscretValue, and _SetImpulsValue underscore-prefixed exports are the
actual shared memory write operations called by the higher-level Set* functions.

libjevent.so maintains two independent event journals: JournalTsCtl for
telestatus control events (1024 entries) and JournalUsoLink for USO link
events (1024 entries). The MaskEvent global filters which events trigger
journal writes. KolRecordInJournal tracks the active journal record count.
OldAddEventProtokol is a legacy event logging path preserved for backward
compatibility with older protocol software.

libterm.so implements a complete local HMI panel driver. The panel uses
/dev/term_kbd for keyboard input and /dev/term_lcd for display output. The
menu system supports up to 5 submenus (PodMenu0 through PodMenu4) organized
under MainMenu. The display supports cursor positioning, LED control,
and multiple string display modes including fixed and floating-point
formatted values. The Kbd and Term functions run as independent threads,
with Kbd processing keyboard input via ionotify and sending MsgSendPulse
notifications to the main Term thread.


## Open Questions

The following items remain incompletely characterised.

kvitok frame exact layout: SendKvitok takes SOST_PRIEM, SOST_SEND, two
  buffers, a uint16, RTOS_RETRANSLATE_ADR, and another uint16. The byte-
  level construction has not been disassembled. The shared library function
  _MakeKvitok in libsystypes.so.1 likely contains the actual frame builder.
  Priority target for the next disassembly round.

cusotm CRC accumulator vtable slot: The accumulation function called via
  vtable[+0x11c] in TestPriem is the per-byte CRC accumulator. Two candidate
  algorithms are implemented in crc16-reconstructed.c. Confirming which one
  requires disassembly of the vtable slot at that offset in the cusotm vtable.

RaspakOldAnalog type byte: The type byte checked by RaspakOldAnalog at
  0x804ac0e requires disassembly. Known: SendZaprosAnalog tries RaspakAnalog
  first (type 0x5a) and on failure falls back to RaspakOldAnalog.

RaspakAddAnalog type byte: The type byte checked by RaspakAddAnalog at
  0x804b0e6 requires disassembly.

UKD command type: The GetUkdValue, MakeKeyInfo, RaspakUkd, RaspakKvitUkd,
  and SendZaprosUkd functions implement an undocumented command type with
  KEY_INFO struct processing, suggesting keyed or authenticated control
  commands. The frame format and type byte require full disassembly.

SERPR pachka framing: The exact wire format of the SERPR pachka batch
  retranslation frames is partially characterised. Priority target for
  future analysis.

encrypt17 in Aqalpha: The three-pointer signature differs from the two-pointer
  encrypt in qalfat. Whether this is a 17-round DES variant, a 17-block
  cipher, or a different algorithm entirely requires disassembly of the
  function body.

cusom type byte assignments: The type bytes at buf[1] used by ZaprosTs
  (0x57), ZaprosSetTi (0x6b), ZaprosSerNom (0x5f), ZaprosTestTs (0x63) are
  confirmed from disassembly. The remaining cusom command type bytes for
  ZaprosTi and ZaprosImpuls require confirmation.

FCRCM in qpuso: The FCRCM variant present in qpuso (pusclass) differs from
  FCRC18 (qalfat) and FCRC16 (qmir, qptym). Its exact construction requires
  disassembly of the FCRCM function body.


## Corrections to Earlier Documentation

This section records specific factual errors that appeared in earlier
versions of the documentation, to prevent them from being reintroduced.

Analog response type byte: Earlier documentation stated 0x86 as the type
  byte for the USOTM analog response. This is incorrect. The disassembly
  of RaspakAnalog at address 0x804ad62 contains the instruction
  cmp BYTE PTR [edi+0x1], 0x5a. The correct analog type byte for the new-
  format parser is 0x5a.

Thread synchronisation path location: Earlier documentation stated that the
  producer/consumer thread synchronisation using mutex mtx at 0x804eec8 and
  condition cond at 0x804eed0 is triggered in RaspakAddAnalog when the
  analog channel count exceeds 55. This is incorrect. The thread sync path
  is in RaspakImpuls at 0x804b3da, where it is used to signal the impulse
  consumer thread after frame validation. The analog parsers do not use this
  mutex/condition pair.

c_oflag masking in InitPort: Earlier documentation listed a c_oflag mask
  of 0xffff7c04 in the InitPort termios configuration. No c_oflag modification
  appears in the disassembly of InitPort at 0x804c9ae. The fields actually
  modified are c_iflag (twice), c_cflag, and c_lflag. The 0xffff7c04 mask
  value applies to c_lflag, not c_oflag.

Aqalpha base class: Earlier documentation described Aqalpha as built on the
  aclass base. This is correct. However, earlier notes also suggested a
  relationship to pusclass or the RTU class polling hierarchy. The aclass
  constructor takes SUPPORTED_USO_AND_BUF_TYPES which is distinct from both
  the usodrv subclass constructors (unsigned short) and the kanaldrv subclass
  constructors. aclass operates at a higher level than either hierarchy.

qalfat class name: The binary is named qalfat but the class is altclass.
  Earlier notes used these names inconsistently. qalfat is the binary file
  name; altclass is the C++ class name as recovered from nm.

tcpqnx source file: The progr/tcpqnx binary's strings section shows the
  source file as qnxgprs.cc rather than tcpqnx.cc. This confirms the binary
  is the GPRS-specific TCP channel driver built from the GPRS source, which
  reuses the ctcpqnx class but in the context of the GSM/GPRS network path.
  The progr/gsmlink binary uses csercom subclassed as cgsmlink for the AT
  command level, while tcpqnx handles the TCP socket level.


## Notable Findings

Firmware typo preserved across releases: The version string "[VERINFO] Aqlpha
  ARM ver 3.6" in Aqalpha contains Aqlpha instead of Aqalpha. This typo
  appears to have been present since the original authoring of the version
  string and was never corrected across at least the 3.6 and 3.7 releases.

Device model codes in Aqalpha: The string section of Aqalpha contains over
  20 device model codes in the format NNNNNN(XX) representing specific
  substation relay protection and metering device models. These codes identify
  the specific hardware that the RTU class polling drivers are designed to
  interface with. The presence of these codes in the supervisor rather than
  in individual driver binaries suggests the supervisor maintains a device
  type registry used for protocol selection at runtime.

comdirect backslash path: The path "\dev\ser1" in comdirect.cc with Windows-
  style backslash separator is unique among all 78 firmware binaries. Every
  other binary uses forward-slash paths. This is the only evidence of a
  non-QNX origin for any component in the firmware set.

Build marker suffix encoding: The suffix characters appended to NIAM in the
  build marker (none, greater than, 5, opening brace, comma, at-sign, 1,
  hash, dot) appear to be single-character unit-type or configuration
  discriminators. Their values span printable ASCII from comma (0x2c) through
  opening brace (0x7b), with no obvious linear encoding. The exact mapping
  from suffix character to firmware role remains unknown.

tcparm uses libkanalarm.so.1: The alarm TCP channel driver links against
  libkanalarm.so.1 rather than libkanaldrv.so.1. This library has not been
  recovered as a separate file in the firmware image, suggesting it may be
  loaded from a different firmware partition or is embedded within tcparm
  itself via static linking.

pdebug presence: The QNX GDB-protocol debug daemon pdebug is present in
  the firmware set, built May 20, 2009, version 6.4.1. Its presence confirms
  the deployed firmware retained full remote debugging capability. The daemon
  can be accessed over TCP/IP on a configurable port or via serial connection.
  Its presence is consistent with the firmware being a development or field-
  maintainable build rather than a hardened production image.


## Redaction Log

The following specific values have been redacted from all published materials:

  IP addresses appearing in start.ini Kanal= and Uso= parameter lines
  IP addresses in restore.scr dev-udp -i and -r parameters
  Unit address values where they correspond to operational substation topology
  Any serial numbers that appeared in binary strings output

No cryptographic key material was found in the strings output of any binary.
The keys array at 0x80785a0 in Aqalpha contains the DES key schedule in the
data segment but no raw key bytes are recoverable from static strings analysis.
No credentials appear in configuration files. The modem.cfg Apache
configuration references a web server at /flashdisk/apache but no httpd.conf
content was captured in the analysis. The des and Permutation functions in
Aqalpha and the encrypt function in qalfat implement DES-based frame
authentication for the RTU class polling protocols, but the key material used
at runtime is loaded from the device configuration rather than being hardcoded.
