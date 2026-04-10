# Russian SCADA ASTUE RTU Custom Protocol Dump

Reverse engineering of a proprietary Russian industrial telemetry protocol
stack found in a QNX Neutrino based SCADA RTU. Research conducted with
explicit written permission from NCIRCC (National Coordination Center for
Computer Incidents). All IP addresses and operationally sensitive identifiers
have been fully redacted.

Read the LICENSE before examining any files.

## What This Is

A PC/104 form factor RTU running QNX 4.x/Neutrino with a layered proprietary
protocol stack referred to internally as RTOS (Distributed Telemetric
Operating System). The device is deployed in Russian power grid substation
infrastructure and bridges multiple field bus protocols to a SCADA master over
TCP/IP WAN, UDP retranslation, and a VHF packet radio backup channel.

## Protocol Overview

The core link layer is built around a snapshot framing model called a srez
(literally "slice"). Each frame begins with a fixed magic byte and carries a
timestamped collection of typed I/O values. The CRC algorithm, escape rules,
and command dispatch table have all been fully reconstructed.

Frame structure (confirmed):

```
Offset   Size   Value    Field
0        1      0x75     Magic byte (ASCII 'u', standing for USO)
1        6      varies   Timestamp, 6 bytes, BCD encoded
7        1      0x82     Frame type discriminator
8        2      0x0000   Reserved
10+      N      varies   Typed I/O payload
```

The USOTM field bus (serial, 115200 baud) uses a separate framing scheme with
a 2 byte CRC appended at frame end and a type byte at offset 1 that identifies
the response class. Minimum valid frame length is 6 bytes. The checksum is
verified by iterating bytes 1 through len-3, calling an internal accumulation
function, then comparing the high and low bytes of the result against bytes
at positions len-2 and len-1.

Response type bytes recovered:

```
0x57   Discrete poll response
0x56   Impulse counter response
0x5D   Internal temperature response
0x86   Analog values response
```

## Protocol Family Covered

| Binary           | Protocol           | Transport      | Purpose                         |
|------------------|--------------------|----------------|---------------------------------|
| libkanaldrv.so.1 | RTOS link layer    | TCP or UDP     | RTU to SCADA master             |
| usotm            | USOTM field bus    | RS-232 serial  | RTU to USO field devices        |
| ekra             | IEC 60870-5-101    | RS-232         | EKRA relay protection IEDs      |
| kpris            | KPRIS proprietary  | UDP serial     | Substation automation           |
| sirius_mb        | Modbus RTU         | RS-485         | Sirius relay IED family         |
| mdbf / mdbtcp    | Modbus RTU and TCP | serial or TCP  | Generic field devices           |
| serpr / cserpr   | SERPR proprietary  | RS-232         | Energy meter / srez retransl    |
| stem300 / cmdbf  | STEM-300 / cmdbf   | RS-232         | Meter family                    |
| p104send         | IEC 60870-5-104    | TCP            | Upstream IEC 104 server         |


## Status

| Area                          | Status              |
|-------------------------------|---------------------|
| RTOS frame header             | Complete            |
| RTOS command opcode table     | Complete (14 ops)   |
| kanaldrv object layout        | Complete            |
| RTOS_RETRANSLATE_ADR struct   | Complete            |
| I/O type strides              | Complete            |
| CRC algorithm (USOTM)         | Complete            |
| USOTM frame format            | Complete            |
| USOTM response type bytes     | Complete            |
| KPRIS command strings         | Complete            |
| IEC 60870-5-101 link layer    | Complete            |
| Modbus CRC tables recovered   | Complete            |
| Timestamp format (6 bytes)    | Confirmed BCD       |
| SOST_PRIEM full layout        | Partial             |
| cusotm AddKod encoding        | Partial             |
| SERPR srez retranslation      | Partial             |
| IEC 60870-5-104 (p104send)    | Partial             |

## Key Binaries by Size

The largest binaries carry the most logic. For reference:

```
Aqalpha          181440 bytes    Main RTU supervisor (qmicro equivalent)
libservdrv.so.1  116140 bytes    QNX IPC service driver
qmicro            62611 bytes    Core measurement and calculation engine
kpris             49483 bytes    KPRIS substation protocol driver
ekra              39315 bytes    IEC 60870-5-101 relay protection driver
libkanaldrv.so.1  40204 bytes    RTOS link layer channel driver
```

## Methodology

All analysis is static only. No live systems were accessed. No network
connections were made to operational infrastructure. Techniques used:

  strings(1) for symbol and string recovery from non-stripped ELF binaries
  nm with C++ demangling for full class and method signature extraction
  objdump for x86 disassembly of critical functions
  Manual annotation of QNX IPC patterns (MsgSend, MsgReceive, shared memory)
  Configuration file semantic analysis (start.ini, modem.cfg, restore.scr)

## Authorization

Research conducted and published with explicit permission from NCIRCC
(National Coordination Center for Computer Incidents), the Russian national
CERT operating under the FSB. All network addresses and operationally
sensitive identifiers have been removed or replaced.
