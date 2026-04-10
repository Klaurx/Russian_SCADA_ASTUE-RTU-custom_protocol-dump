# Russian SCADA ASTUE RTU Custom Protocol Dump

Reverse engineering of a proprietary Russian industrial telemetry protocol
stack recovered from a QNX Neutrino based SCADA remote terminal unit.
Research conducted with explicit written permission from NCIRCC (National
Coordination Center for Computer Incidents). All IP addresses and
operationally sensitive identifiers have been fully redacted before
publication.

Read the LICENSE before examining any files.

Also guys, sadly i don't think there will be any updates on this specific
project, due to the fact im working on this as 1 person, and dealing with
school etc. at the same time, it is very difficult to reverse engineer a
full SCADA RTU firmware dump all alone. Sorry. But you can always check
out other cool, simpler stuff on my page.


## What This Is

A PC/104 form factor RTU running QNX 4.x/Neutrino with a layered proprietary
protocol stack referred to internally as RTOS. The exact expansion of the
acronym is not recoverable from the binary strings but context throughout
the firmware consistently uses it to mean the distributed telemetric link
layer. The device is deployed in Russian power grid substation infrastructure
and bridges multiple field bus protocols to a SCADA master over TCP/IP WAN,
UDP retranslation, and a VHF packet radio backup channel. A GSM/GPRS path
also exists as a quaternary fallback.

The firmware image contains 78 distinct binaries across 7 directories. None
of them are stripped. Every class name, method name, parameter type, and
vtable layout is recoverable via nm with C++ demangling. The non-stripped
state is the single most valuable property of this firmware for analysis
purposes.


## Hardware Platform

Form factor: PC/104 stack with an x86 DX variant processor board.

Operating system: QNX Neutrino, confirmed by the presence of qconn,
MsgSend/MsgReceive IPC primitives, pthread_sleepon primitives, sysmgr_reboot,
the devf-sram flash device driver, and the QNX resource manager framework
throughout the binary set.

CPU architecture: Intel i386, all ELF binaries are 32-bit LSB.

Storage hierarchy: /flashdisk (primary flash), /sramdisk (SRAM backed
volatile storage), /flashdisk/swop (swap area for ring buffer persistence).

All binaries were compiled with GCC 4.3.3 targeting QNX Neutrino, confirmed
by the compiler identification strings present in every binary.

Hardware components identified from binary names and symbol analysis:

  gpio86dx is the PC/104 digital I/O board driver for the x86 DX variant
  amux and amux86dx are the analog multiplexer drivers for the PC/104 board
  amux86dx adds the SetAdr method for addressable channel selection on the
    x86 DX variant board, which the plain amux variant lacks
  c5600 is the driver for the c5600 PC/104 ISA digital I/O board, which
    uses a different ISA register layout and separate Opros5600 and Opr
    threads for concurrent polling
  libwatch86dx.so is the hardware watchdog driver for the x86 DX board
  libwatch586.so is an alternative watchdog for 586 class hardware variants
  devf-sram is the QNX flash and SRAM device driver
  sram_format is the SRAM formatting utility
  gpstime is the GPS receiver daemon responsible for time synchronisation
  infogps is a GPS status query utility
  modem_gsm and the cgsmlink class handle the GSM modem via AT+CFUN=1
  rmodem is the TNC packet radio modem resource manager
  power exports PowerON() and uses ThreadCtl for GPIO power relay control
  p104send implements the upstream IEC 60870-5-104 TCP server


## Protocol Overview

The core link layer is built around a snapshot framing model called a srez,
which translates literally as slice. Each frame begins with a fixed magic
byte and carries a timestamped collection of typed I/O values. The CRC
algorithm, escape rules, and command dispatch table have all been fully
reconstructed from disassembly.

Srez frame header structure, confirmed from disassembly of MakeSrezBuf and
MakeSrezWithFictiveTime:

```
Offset   Size   Value    Field
0        1      0x75     Magic byte, ASCII u, from USO
1        6      varies   Timestamp, 6 bytes BCD encoded YY MM DD HH MM SS
7        1      varies   Frame type discriminator
8        2      0x0000   Reserved, always zero
```

Frame type discriminator values recovered from MakeSrezBuf:

```
0x82   Fictive timestamp buffer, synthesised when GPS is unavailable
0x7f   Normal data buffer
0x01   Discrete type sub-frame
0x02   Analog type sub-frame
0x09   Impulse and counter sub-frame
```

The USOTM field bus runs over a serial port at 115200 baud. It uses a
separate framing scheme with a 2-byte CRC appended at the frame tail and
a type byte at offset 1 that identifies the response class. Minimum valid
frame length is 6 bytes. Frame validity is checked by iterating bytes 1
through len-3, calling a virtual accumulation function, then comparing the
high and low bytes of the result against the two bytes at positions len-2
and len-1.

Response type bytes recovered from disassembly of each Raspak function:

```
0x57   Discrete poll response
0x56   Impulse counter response
0x5a   Analog values response (new format, RaspakAnalog, 32 channels)
0x5d   Internal temperature response, TempVn
```

Note that a separate older analog format exists handled by RaspakOldAnalog
which processes 8 channels and uses a different type byte. RaspakAddAnalog
handles a third analog variant used for the AddAnalog poll cycle. The
documentation in earlier versions of this repository incorrectly listed
0x86 as the analog type byte and incorrectly attributed the producer/consumer
thread synchronisation path to the analog parser rather than the impulse
parser where it actually resides.


## Protocol Family Covered

```
Binary             Protocol               Transport        Purpose
libkanaldrv.so.1   RTOS link layer        TCP or UDP       RTU to SCADA master
usotm              USOTM field bus        RS-232 serial    RTU to USO devices
usotmj             USOTM with SOE journal RS-232 serial    Change-driven variant
usom               USOM field bus         RS-232 serial    Third USO bus class
usom2              USOM extended          RS-232 serial    Serial number and test
ekra               IEC 60870-5-101        RS-232           EKRA relay IEDs
kpris              KPRIS proprietary      RS-232 or UDP    Substation automation
sirius_mb          Modbus RTU             RS-485           Sirius relay IED family
mdbf               Modbus RTU             serial           Generic field devices
mdbf80             Modbus RTU enhanced    serial           Extended Modbus variant
mdbfo              Modbus RTU open        serial           Open Modbus variant
mdbtcp             Modbus TCP             TCP              Generic TCP Modbus
mdbrturetr         Modbus RTU retr        serial           RTU retranslation path
mdbtcpretr         Modbus TCP retr        TCP              TCP retranslation path
serpr              SERPR proprietary      RS-232           Srez retranslation
stem300            STEM-300 / CMDBF       RS-232           Meter family
p104send           IEC 60870-5-104        TCP              Upstream IEC 104 server
cicpcon            ICP-CON protocol       RS-485           ICP-CON devices
rpn                RPN protocol           RS-232           Stepped relay RTU class
qalfat             altclass protocol 18   RS-232           DES-encrypted RTU class
qcet               ctclass polling        RS-232           Class-based RTU polling
qmir               mirclass polling       RS-232           MIR variant RTU polling
qpuso              pusclass polling       RS-232           PUS variant RTU polling
qpty               pclass polling         RS-232           PTY variant RTU polling
qptym              ptmclass polling        RS-232           PTM variant RTU polling
Aqalpha            aclass supervisor      TCP              Primary application host
c5600              uso5600                ISA direct       PC/104 c5600 board
comdirect          comdirect              RS-232           Raw COM port driver
gpio86dx           usodrv GPIO            ISA direct       PC/104 digital I/O
amux               uso_amux               ISA LPT          Analog multiplexer
amux86dx           uso_amux x86DX         ISA LPT          Addressable multiplexer
tcpqnx             ctcpqnx                TCP              Primary TCP channel
tcparm             ctcpqnx (alarm)        TCP              Alarm TCP channel
udpqnx             cudpqnx                UDP              Primary UDP channel
udpretransl        cudpretr               UDP              UDP retranslation
sercom             csercom                RS-232           Serial channel base
gsmlink            cgsmlink               GSM              GSM modem channel
tnc                ctnc                   VHF radio        AX.25 packet radio
rs485retransl      crs485retr             RS-485           RS-485 retranslation
rs485dosretransl   crs485retr (DOS)       RS-485           DOS mode retranslation
```


## Status

```
Area                             Status
RTOS frame header                Complete
RTOS command opcode table        Complete, 14 opcodes
kanaldrv object layout           Complete
RTOS_RETRANSLATE_ADR struct      Complete
I/O type strides                 Complete
CRC algorithm USOTM              Complete, two candidates, Modbus or CCITT
USOTM frame format               Complete
USOTM response type bytes        Corrected, 0x5a for analog not 0x86
cusotm Working loop sequence     Complete, 11-step poll cycle documented
cusotm tag flag offsets          Complete, all five pending flags documented
KPRIS command strings            Complete
IEC 60870-5-101 link layer       Complete
Modbus CRC tables recovered      Complete
Timestamp format 6 bytes         Confirmed BCD
RaspakDiscret                    Complete
RaspakImpuls                     Complete, thread sync path confirmed here
RaspakAnalog type 0x5a           Complete, 32 channels
RaspakOldAnalog                  Complete, 8 channels
RaspakAddAnalog                  Complete
RaspakTempVn                     Complete
RaspakTempNar                    Complete
UKD command type                 Partial
SOST_PRIEM full layout           Partial
cusotm AddKod encoding           Complete
SERPR srez retranslation         Partial
IEC 60870-5-104 cp104send        Complete, all ASDU types documented
altclass crypto (qalfat)         Complete, FCRC18 and DES fully reconstructed
altclass USA struct layout       Complete, 17 confirmed fields
altclass fopimpt state machine   Complete, full session states documented
altclass fend_send               Complete, CRC appended to outbound frames
aclass (Aqalpha)                 Complete symbol table, methods characterized
aclass crypto (encrypt17)        Identified, distinct from qalfat encrypt
cusom vs cusom2 differences      Complete
usotmj journal extension         Complete
pusclass (qpuso)                 Complete symbol table
mirclass (qmir)                  Complete symbol table
ctclass (qcet)                   Complete symbol table
pclass (qpty)                    Complete symbol table
ptmclass (qptym)                 Complete symbol table
crpn (rpn)                       Complete symbol table
ctnc (tnc)                       Complete symbol table
cicpcon (icpcon)                 Complete symbol table
cmdbf stem300 variant            Complete symbol table
cmdbf mdbf80 variant             Complete symbol table
cmdbf mdbfo variant              Complete symbol table
uso5600 (c5600)                  Complete symbol table
uso_amux (amux)                  Complete symbol table
uso_amux x86DX (amux86dx)        Complete symbol table
comdirect                        Complete symbol table
csercom (sercom)                 Complete symbol table
cgsmlink (gsmlink)               Complete symbol table
ctcpqnx (tcpqnx)                 Complete symbol table
ctcpqnx alarm (tcparm)           Complete, uses libkanalarm.so.1
cudpqnx (udpqnx)                 Complete symbol table
cudpretr (udpretransl)           Complete symbol table
crs485retr (rs485retransl)       Complete symbol table
crs485retr DOS (rs485dosretransl) Complete, uses libretransldos.so.1
librashet.so                     Complete symbol table, all exports documented
libjevent.so                     Complete symbol table, journal structure documented
libterm.so                       Complete symbol table, HMI panel documented
libtmkorr.so                     Complete
progr/start.ini                  Complete, fully recovered
project/start.ini                Complete, fully recovered
addprog/restore.scr              Complete, fully recovered
addprog/modem.cfg                Complete, fully recovered including comments
sbat/preinit                     Identified, hardware abstraction layer init
Aqalpha supervisor binary        Complete symbol table, aclass characterised
sqmicro watchdog                 Complete
fixuso virtual USO               Complete
usoimit simulator                Complete
pdebug                           Complete, QNX GDB protocol debugger
flashctl                         Complete, flash filesystem control utility
restore binary                   Complete, ParseScr and Process characterised
power binary                     Complete, PowerON via ThreadCtl
```


## Key Binaries by Size

The largest binaries carry the most logic. Sizes are text and data sections
combined, not the full virtual memory image.

```
Aqalpha          181440 bytes   Main RTU supervisor, aclass base
libservdrv.so.1  116140 bytes   QNX IPC service driver
qmicro            62611 bytes   Core measurement and calculation engine
kpris             49483 bytes   KPRIS substation protocol driver
qcet              51966 bytes   Class-based RTU polling driver
ekra              39315 bytes   IEC 60870-5-101 relay protection driver
libkanaldrv.so.1  40204 bytes   RTOS link layer channel driver
qalfat            varies         altclass DES-encrypted RTU protocol driver
```

The Aqalpha binary carries the version strings "Aqalpha ver 3.7 arm" and
"[VERINFO] Aqlpha ARM ver 3.6" in its strings section. Note that the second
string contains a typographical error in the original firmware: Aqlpha instead
of Aqalpha. This appears to be a long-standing typo in the firmware version
reporting code. The launch format string "Aqalpha ver 3.7 arm p/dev/ser[N]
s[speed] d[d] ...." confirms that Aqalpha accepts a serial port path, baud
rate, and a mode discriminator as command line arguments.


## Build Timestamps

All binaries share a common build date of December 9, 2024 at 18:33,
representing the main firmware release. Notable exceptions are listed below.

```
addprog/rmodem        December 25, 2023   older radio modem driver
addprog/dev-udp       December 25, 2023
addprog/gpstime       December 25, 2023
addprog/infogps       December 25, 2023
addprog/tcpsrv        July 3, 2025        newest binary in the set
project/start.ini     July 3, 2025        updated deployment configuration
progr/start.ini       December 15, 2024   active deployment configuration
```

The tcpsrv binary is a TCP-to-serial bridge added as a firmware update.
Its debug strings confirm bidirectional relay: "Start serial thread port=%ld"
and "Start Tcp thread %ld". It calls procmgr_daemon to daemonise itself.

The qalfat binary carries the version string "[VERINFO] qalfat ver 1.8"
confirming it is a versioned protocol driver in the same firmware release
family.

Version strings recovered from the RTU polling driver binaries:

```
qalfat    [VERINFO] qalfat ver 1.8
qpuso     [VERINFO] qpuso ver 1.0
qmir      [VERINFO] qmir ver 1.1
qpty      [VERINFO] pty ver 3.0 ARM
qptym     [VERINFO] qptym ver 5.0
```


## Build Marker Convention

Every binary in the firmware set ends its strings section with a marker of
the form NIAM followed by SRWQV, sometimes with a suffix character such as
X, T, j, 5, or z appended to NIAM. NIAM is MAIN reversed. The suffix
characters appear to be unit-type discriminators baked into each binary at
build time. SRWQV has not been decoded but appears consistently as a second
marker in the same position across all binaries.

Suffix variants observed across the firmware:

```
NIAM      plain, no suffix, most common variant
NIAM>     qalfat
NIAM5     qmir
NIAM{     usom2
NIAM,     rpn, crpn
NIAM-     restore
NIAM@     rs485retransl and rs485dosretransl
NIAM1     flashctl
NIAM#     mdbf80
NIAM.     mdbrturetr or related Modbus variant
```


## Methodology

All analysis is static only. No live systems were accessed. No network
connections were made to operational infrastructure.

Techniques used:

  strings on all ELF binaries, high yield due to non-stripped state
  nm with C++ demangling for full class and method signature extraction
  objdump for x86 disassembly of targeted functions
  Manual annotation of QNX IPC patterns including MsgSend, MsgReceive,
    and shared memory segment usage
  Configuration file semantic analysis of start.ini, modem.cfg, restore.scr


## Authorization

Research conducted and published with explicit permission from NCIRCC
(National Coordination Center for Computer Incidents), the Russian national
CERT operating under the FSB. All network addresses and operationally
sensitive identifiers have been removed or replaced with the string [REDACTED].
