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
    and the netcfg binary strings which contain the restore.scr verbatim.


## Compiler and ABI Notes

All binaries use GCC 4.3.3 targeting i386 QNX Neutrino with position-
independent code (-fPIC). The calling convention is standard i386 cdecl
with the this pointer passed in eax for thiscall variants. Some optimised
functions use esi or edi as the object register instead. C++ name mangling
follows the Itanium ABI which nm with -C demangles cleanly.

Floating point uses the x87 FPU. The fldz, fstp, and fadd patterns are
visible throughout libkanaldrv.so.1 and other binaries. The stub methods
GetStaticTimeSend and GetDinamicTimeSend in the kanaldrv base class both
return 0.0 via fldz followed by ret, confirming they are base class stubs
overridden by transport subclasses.

Full RTTI is enabled across the firmware. Type information structures and
vtables are fully present and demangleable. Class hierarchies are visible
from the typeinfo and vtable symbols without any disassembly.


## Binary Inventory and Classification

The firmware contains well over fifty distinct ELF binaries. The following
classification covers all confirmed binaries:

Core measurement engine:
  qmicro    central measurement and calculation supervisor, QMICRO class
  Aqalpha   primary application supervisor, aclass base, not yet fully analysed

Channel drivers (all implement kanaldrv vtable):
  libkanaldrv.so.1  base class and RTOS frame layer
  ctcpqnx           TCP transport subclass
  csercom           serial transport subclass
  cgsmlink          GSM modem subclass of csercom
  cudpqnx           UDP transport subclass
  cudpretr          UDP retranslation transport subclass
  crs485retr        RS-485 retranslation transport subclass
  ctnc              TNC packet radio transport with full AX.25 support

Retranslation drivers:
  libretransldos.so.1  DOS-mode retranslation, retransldrv class
  libretransldrv.so.1  driver-mode retranslation, retransldrv class

Field bus protocol drivers (all implement usodrv vtable):
  usotm / cusotm      USOTM serial field bus, primary I/O protocol
  ekra / cekra        IEC 60870-5-101 for EKRA relay protection IEDs
  kpris / ckpris      KPRIS substation automation protocol
  sirius_mb           Modbus RTU for Sirius relay IED family
  sirius_mb_ntu       Modbus RTU NTU variant of above
  mdbf                Modbus RTU over serial
  mdbf80              Modbus RTU 80-series variant
  mdbfo               Modbus RTU open variant
  mdbtcp              Modbus TCP
  mdbrturetr          Modbus RTU retranslation
  mdbtcpretr          Modbus TCP retranslation
  serpr / cserpr      SERPR srez retranslation protocol
  cserpr (named as cserpr in binary)  same driver
  stem300 / cmdbf     STEM-300 and CMDBF meter protocols, serial and TCP
  cicpcon             ICP-CON protocol for ICP-CON field devices
  crpn                RPN protocol for specialised RTU class devices
  qcet / ctclass      class-based RTU polling (RTU class polling)
  qmir / mirclass     MIR variant RTU polling
  qpuso / pusclass    PUS variant RTU polling
  comdirect           direct COM port driver
  gpio86dx            PC/104 digital I/O board driver
  uso_amux            PC/104 analog multiplexer driver
  uso5600             c5600 PC/104 board driver
  fixuso              virtual USO in shared memory for testing
  usoimit             USO simulator creating /ConfigImit segments
  p104send            upstream IEC 60870-5-104 TCP server driver

Support libraries:
  libusodrv.so.1    USO data model base class
  libservdrv.so.1   QNX IPC service driver
  librashet.so      engineering calculation engine
  libjevent.so.1    event journaling module
  libtmkorr.so      GPS time correction library
  libsystypes.so.1  shared type definitions
  libwatch86dx.so   hardware watchdog for x86 DX board
  libwatch586.so    hardware watchdog for 586 class hardware
  libterm.so        terminal abstraction for local HMI

Hardware and system utilities:
  rmodem            TNC packet radio resource manager (two variants)
  gpstime           GPS time synchronisation daemon
  infogps           GPS status query utility
  dev-udp           UDP virtual serial port resource manager
  tcpsrv            TCP-to-serial bridge (newest binary, July 2025)
  modem_gsm         GSM modem command utility
  power             GPIO power control utility
  sqmicro           secondary watchdog and permission fixer
  sram_format       SRAM formatting utility
  netcfg            network configuration utility
  devf-sram         QNX flash and SRAM device driver
  flashctl          flash filesystem control utility


## Open Questions

The following items remain incompletely characterised.

SOST_PRIEM layout: Size is confirmed at 0x12 bytes (18) from constructor
  zeroing at obj+0x1d0 in the kanaldrv constructor. The FSM state fields
  have been partially identified but the exact byte offsets of rx_count,
  error_count, and timeout_ms are estimated from context rather than confirmed
  from direct disassembly of PriemPacket.

cusotm::IsFirstByte: This function at 0x804970a is called in WaitOtvet to
  validate the first received byte before accepting a frame. From all outbound
  frame construction observed in cusotm (SendGroupTu, SendZaprosUkd,
  SendTuCommand), the start byte is consistently 0x5b. The inbound start
  byte is very likely also 0x5b, since field devices respond with the same
  framing convention. However, direct disassembly of IsFirstByte is required
  to confirm this rather than infer it.

kvitok frame exact layout: SendKvitok takes SOST_PRIEM, SOST_SEND, two
  buffers, a uint16, RTOS_RETRANSLATE_ADR, and another uint16. The byte-level
  construction has not been disassembled. The shared library function
  _MakeKvitok in libsystypes.so.1 likely contains the actual frame builder.
  Priority target for the next disassembly round.

cusotm CRC accumulator vtable slot: The accumulation function called via
  vtable[+0x11c] in TestPriem is the per-byte CRC accumulator. Two candidate
  algorithms are implemented in crc16-reconstructed.c. Confirming which one
  requires disassembly of the vtable slot at that offset in the cusotm vtable.

RaspakOldAnalog type byte: The type byte checked by RaspakOldAnalog at
  0x804ac0e requires disassembly. It is known that SendZaprosAnalog in
  Working() tries RaspakAnalog first (type 0x5a), and on failure falls back
  to calling SendBuffer with a 5-byte frame and then RaspakOldAnalog.

RaspakAddAnalog type byte: The type byte checked by RaspakAddAnalog at
  0x804b0e6 requires disassembly. It is distinct from both 0x5a and 0x5d.

UKD command type: The GetUkdValue, MakeKeyInfo, RaspakUkd, RaspakKvitUkd,
  and SendZaprosUkd functions implement an undocumented command type with
  KEY_INFO struct processing, suggesting keyed or authenticated control
  commands. The frame format and type byte require full disassembly.

SERPR pachka framing: The exact wire format of the SERPR pachka (packet
  batch) retranslation frames is partially characterised. The length formula
  and type byte structure differ from the srez framing. Priority target for
  future analysis.

p104send IEC 104 server: The cp104send class is well documented by its
  symbol table and implements a complete IEC 60870-5-104 server including
  APDU S, I, and U frame types, general interrogation, time synchronisation,
  and spontaneous transmission. The SrezForRemoteKp function converts RTOS
  srez data into IEC 104 ASDUs for upstream masters. Partially characterised.

Aqalpha supervisor: The Aqalpha binary (181440 bytes) is the largest in
  the firmware set and serves as the primary application supervisor on the
  aclass base. It contains extensive protocol handling including des,
  Permutation, and Decode cryptographic-style functions. The relationship
  between Aqalpha and qmicro in the runtime process hierarchy requires
  further analysis. The version string contains a typo in the original
  firmware: "Aqlpha" instead of "Aqalpha" in one of the two version strings.

Timestamp field ordering: The 6-byte BCD timestamp at srez offsets 1
  through 6 is confirmed as BCD from MakeSrezWithFictiveTime and from
  ConvLocToSerKan in the qcet and Aqalpha binaries. The field ordering
  YY MM DD HH MM SS is the most natural arrangement and is consistent with
  the annotated packet examples, but has not been confirmed from a live
  frame capture or from disassembly of the GPS time conversion path.


## Corrections to Earlier Documentation

This section records specific factual errors that appeared in earlier
versions of the documentation, to prevent them from being reintroduced.

Analog response type byte: Earlier documentation stated 0x86 as the type
  byte for the USOTM analog response. This is incorrect. The disassembly
  of RaspakAnalog at address 0x804ad62 contains the instruction:
    cmp BYTE PTR [edi+0x1], 0x5a
  The correct analog type byte for the new-format parser is 0x5a.

Thread synchronisation path location: Earlier documentation stated that the
  producer/consumer thread synchronisation using mutex mtx at 0x804eec8 and
  condition cond at 0x804eed0 is triggered in RaspakAddAnalog when the
  analog channel count exceeds 55. This is incorrect. The thread sync path
  is in RaspakImpuls at 0x804b3da, where it is used to signal the impulse
  consumer thread after frame validation. The analog parsers do not use this
  mutex/condition pair.

Named globals: Earlier documentation referred to the mutex and condition
  variable by their raw addresses. Both are named globals confirmed from nm:
    0x804eec8   D   mtx
    0x804eed0   D   cond

c_oflag masking in InitPort: Earlier documentation listed a c_oflag mask
  of 0xffff7c04 in the InitPort termios configuration. No c_oflag modification
  appears in the disassembly of InitPort at 0x804c9ae. The fields actually
  modified are c_iflag (twice, using two different mask values), c_cflag,
  and c_lflag. The 0xffff7c04 mask value applies to c_lflag, not c_oflag.


## Notable Findings

sqmicro: Source file sqmicro.cc. The function chmod_all() is the primary
  entry point. It performs recursive permission setting on the flash
  filesystem and then spawns updatepo and qmicro. Its name suggests it
  is a secondary micro watchdog that ensures correct file permissions
  and restarts the main measurement engine if needed.

fixuso: Creates /VirtualUsoSemaphore and /VirtualUsoParametrs shared memory
  objects. Implements a virtual USO device entirely in shared memory. Used
  for testing or for bridging non-serial data sources into the USO data
  model. The fixuso class inherits directly from usodrv.

usoimit: Implements the usoimit class which creates /ConfigImit, /ConfigTs,
  /ConfigTi, /ConfigImp, and /ConfigTu shared memory segments. This is a
  USO simulator that can replay pre-configured I/O patterns for testing
  the qmicro engine without requiring physical hardware.

ctclass / qcet: Version string "[VERINFO] qcet ver 3.3". The ctclass class
  implements class-based RTU polling. Functions include MCRC16, ConvLocTimeSyst,
  ConvLocToSer, ConvLocToSerKan, FCRCPhtc, frtekt, frtekannel, and NEXT_PRIBOR.
  This appears to be a specialised protocol driver for a class of RTU devices
  using a different framing convention from USOTM.

tcpsrv: The newest binary in the firmware set (July 2025). A TCP-to-serial
  bridge that creates a serial port thread and a TCP socket thread, relaying
  data bidirectionally between them. Debug strings confirm:
  "Start serial thread port=%ld" and "Start Tcp thread %ld". It calls
  procmgr_daemon to daemonise itself. Added as a firmware update to support
  web-based configuration access.

netcfg: Contains the complete restore.scr script content in its strings
  section, revealing the full startup sequence including four LAN interfaces,
  the Apache web server path, GPS daemon invocation, and the commented-out
  dev-udp two-interface KPRIS polling configuration.

rmodem: A very large binary implementing a full AX.25 radio packet protocol
  stack with LZW compression and decompression (ShrinkLZW and UnshrinkLZW),
  QNX resource manager registration (rmodem_io_read, rmodem_io_write,
  rmodem_resmgr), and TNC modem control via serial AT commands. The binary
  exists in two variants (rmodem and rmodem_wrk) with slightly different sizes.
  The protocol supports CONNECT, DISCONNECT, and full frame sequencing with
  retry logic.

Build marker: Every binary ends its strings section with NIAM followed by
  SRWQV, sometimes with a suffix character. NIAM is MAIN spelled backwards.
  Suffix characters observed: none (plain NIAM), X, T, j, 5, z, and a comma.
  These appear to be unit-type or configuration discriminators embedded at
  build time by the firmware build system.


## Redaction Log

The following specific values have been redacted from all published materials:

  IP addresses appearing in start.ini Kanal= and Uso= parameter lines
  IP addresses in restore.scr dev-udp -i and -r parameters
  Unit address values where they correspond to operational substation topology
  Any serial numbers that appeared in binary strings output

No cryptographic key material was found anywhere in the firmware. No
credentials appear in configuration files. The restore.scr Apache configuration
references a web server at /flashdisk/apache but no httpd.conf content was
captured in the analysis. The des() and Permutation() functions in Aqalpha
and related binaries appear to implement DES-based frame authentication for
specific RTU class polling protocols, but no keys are present in the strings.
