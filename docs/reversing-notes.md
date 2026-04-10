# Reversing Notes

## Methodology

All analysis is static. No dynamic instrumentation, no live system access,
no network connections to operational infrastructure.

Primary tools and techniques:

  strings(1) on all ELF binaries (non-stripped, high yield)
  nm with C++ demangling for full class and method signature extraction
  objdump -d -M intel for x86 disassembly of targeted functions
  Manual annotation of QNX IPC patterns
  Configuration file semantic analysis

The absence of stripping is the single most valuable property of this firmware.
Every class name, method name, and parameter type is recoverable. GCC 4.3.3
with full RTTI enabled means even type hierarchy and vtable layouts are legible.

## Compiler and ABI Notes

All binaries use GCC 4.3.3 targeting i386 QNX Neutrino with -fPIC. The
calling convention is standard i386 cdecl with this pointer passed in eax
for thiscall variants (some functions use esi or edi as the object register
in optimised forms). C++ name mangling follows the Itanium ABI which nm -C
demangles cleanly.

Floating point uses the x87 FPU (fldz, fstp, fadd patterns visible throughout
kanaldrv). The GetStaticTimeSend and GetDinamicTimeSend stubs both return
0.0 via fldz/ret, confirming base class stubs overridden by subclasses.

## Open Questions

The following items remain incompletely characterised.

SOST_PRIEM layout. Size confirmed at 0x12 bytes (18) from constructor
zeroing at obj+0x1D0. The FSM state fields have been partially identified
but the exact byte offsets of rx_count, error_count, and timeout_ms are
estimated from context rather than direct disassembly.

cusotm::IsFirstByte. This function at 0x804970A is called in WaitOtvet to
validate the first received byte before accepting a frame. The exact magic
byte value it checks is not in the strings output and requires disassembly.
It likely checks for 0x5B or similar fixed start byte.

kvitok frame exact layout. SendKvitok takes SOST_PRIEM, SOST_SEND, two
buffers, a uint16, RTOS_RETRANSLATE_ADR, and another uint16. The byte-level
construction has not been disassembled. Priority target for next round.

cusotm::AddKod encoding detail. The vtable call at [ecx+0x11C] in the CRC
accumulation loop of TestPriem is the per-byte accumulator. Its exact
polynomial or table structure is unknown without disassembling that vtable
slot. Likely CRC16 CCITT or a custom accumulation function.

The 6-byte timestamp at srez offsets 1-6. Confirmed as BCD from the
MakeSrezWithFictiveTime analysis (memcpy of 6 bytes from TIME_SERVER_KANAL).
The exact field ordering (YY/MM/DD/HH/MM/SS vs other arrangement) requires
either disassembly of the GPS time conversion functions or capture of a live
frame.

cserpr (serpr binary). The SERPR protocol appears to be a srez retranslation
protocol that bridges between the kanaldrv layer and upstream consumers.
It implements MakeZaprosKvitEvent, ZaprosEvent, RaspakSrez, RaspakExtSrez,
MakeNextPachka, ReadPachkaOtvet, AnalizPachkaOtvet. The word pachka (packet/
batch) appears frequently, distinct from the srez framing. This may be an
intermediate buffering and retranslation protocol separate from the main RTOS
link layer. Partially characterised.

p104send IEC 104 server. The cp104send class is well-documented by its
symbol table and implements a complete IEC 60870-5-104 server (APDU S/I/U
frame types, GI, time sync, spontaneous transmission). The SrezForRemoteKp
function suggests it converts RTOS srez data into IEC 104 ASDUs for upstream
masters that speak IEC 104. Partially characterised.

## Notable Findings Not in Main Docs

The Aqalpha binary (181440 bytes, largest in the repo) is not yet analysed.
Its name suggests it is the primary application supervisor, possibly the
main() entry point for the entire RTU process. It is larger than libservdrv
which is the core IPC library, implying it contains significant logic.

sqmicro and sqmicro.cc implement a simple utility that runs chmod and
sysmgr_reboot. Its name suggests it is a secondary micro watchdog that
ensures filesystem permissions and triggers a clean reboot.

fixuso creates two named POSIX SHM objects /VirtualUsoSemaphore and
/VirtualUsoParametrs. This implements a virtual (simulated) USO device in
shared memory, used for testing or for bridging non-serial data sources into
the USO data model.

qcet version string recovered: "[VERINFO] qcet ver 3.3". qcet is one of the
larger binaries (51966 bytes) and implements the ctclass class which shares
naming with a class hierarchy related to RTU class (klasse) polling. The
strings reveal it manages: AlphaUso, a CRC function MCRC16, time conversion
functions (ConvLocTimeSyst, ConvLocToSerKan, ConvLocToSer), framing functions
(FCRCPhtc, frtekt, frtekannel). This appears to be a specialised protocol
driver for a class of RTU devices using a different framing than USOTM.

The tcpsrv binary (Jul 3 2025, newest file) is a TCP-to-serial bridge that
creates both a serial port thread and a TCP socket thread, relaying data
bidirectionally. The debug strings "Start serial thread port=%ld" and "Start
Tcp thread %ld" confirm this. It calls procmgr_daemon to daemonise. This was
likely added as a firmware update to support the web-based configuration path.

## Redaction Log

The following specific values have been redacted from all published materials:

  IP addresses appearing in start.ini Kanal= and Uso= lines
  IP addresses in restore.scr dev-udp -i and -r parameters
  Unit address values where they correspond to operational topology
  Any serial numbers that appeared in strings output

No cryptographic material was found anywhere in the firmware. No credentials
were found in configuration files. The restore.scr web server configuration
referenced an Apache instance but no httpd.conf content was captured.
