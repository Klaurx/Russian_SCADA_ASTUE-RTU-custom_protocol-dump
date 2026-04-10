# System Architecture

## Hardware Platform

Form factor: PC/104 stack, x86 DX variant board.
Operating system: QNX Neutrino (confirmed by qconn, MsgSend/MsgReceive IPC,
pthread_sleepon primitives, sysmgr_reboot, and devf-sram driver).
CPU architecture: Intel i386, all ELF binaries are 32-bit LSB.
Storage: /flashdisk (flash), /sramdisk (SRAM), /flashdisk/swop (swap area).

All binaries compiled with GCC 4.3.3 targeting QNX Neutrino.

Hardware components identified from binary names and symbol analysis:

```
gpio86dx          PC/104 digital I/O board (x86 DX variant)
amux / amux86dx   Analog multiplexer for PC/104 board
libwatch86dx.so   Hardware watchdog driver for x86 DX board
libwatch586.so    Alternative watchdog for 586 class hardware
devf-sram         QNX flash/SRAM device driver
sram_format       SRAM formatting utility
gpstime           GPS receiver daemon (time synchronisation)
infogps           GPS status query utility
modem_gsm         GSM modem driver
rmodem            TNC packet radio modem driver
p104send          IEC 60870-5-104 TCP server driver
```

## Communication Paths

Primary: TCP/IP WAN via tcpqnx (ctcpqnx class).
RTU binds [REDACTED]:2124, connects to master at [REDACTED]:5124.
Unit address 111 (from start.ini a=111 parameter).

Secondary: UDP retranslation via udpretransl on port 2127 (project config).

Tertiary: VHF packet radio via rmodem and tnc.
Physical: /dev/ser4 at 19200 baud connected to TNC modem.
Backup communications path for remote substations without reliable WAN.

Quaternary: GSM/GPRS via gsmlink and qnxgprs.

Field bus: usotm on /dev/ser7 (active config) or /dev/ser9 (project config)
at 115200 baud. Memory-mapped local I/O via gpio86dx.

## Software Stack

```
GPS time source (gpstime, infogps)
          |
          v
libtmkorr.so  time correction (ConnectToGps, IsTimeKorrectEnable)
          |
          v
libkanaldrv.so.1  RTOS link layer (kanaldrv class, initkandrv subclass)
  RX:  ReadByteFromPort -> PriemPacket -> AnalizBufPriem
  TX:  MakeSrezBuf / MakeAdrBuf -> SendBuffer
  ACK: SendKvitok / ReadKvitok
  SHM: /Sem%.04u /SB%.04u /BE%.04u /BP%.04u Nreg%.04u Per%.04u
          |
     +----+--------+--------+
     |             |        |
  usotm          ekra     kpris
  cusotm class   cekra    ckpris
  /dev/ser7      IEC 101  substation
  115200 baud    RS-232   automation
     |             |        |
     +----+--------+--------+
          |
          v
libusodrv.so.1  USO data model
  I/O types:   iocDiscret, iocAnalog, iocImpuls, iocDout, iocuso
  Config SHM:  Discrets, Analogs, Impuls, Douts, Uso, Formula, Constants
  Registration: AddUserDataDiscret/Analog/Impuls/Dout/Uso
          |
          v
librashet.so  engineering calculation engine
  Signal:  TestGran, TestIn, TestMaskBit, vfabs, SQRT
  Timers:  InitTimer, NextStepTimer, InitShimTimer, NextStepPulsTimer
  Relay:   ReadSostReley, GroupAnaliz, GroupAnaliz1, GroupAnaliz2
  Values:  SetAnalog, SetPrValueRA, RashetAnalog, RashetDiscret
          |
          v
libservdrv.so.1  QNX IPC service driver (ServModuleInit, LoadServDrv)
  IPC: MsgSend, MsgReceive, MsgReply
          |
          v
Retranslation layer
  libretransldos.so.1 / libretransldrv.so.1
  mdbtcp / mdbtcpretr  (Modbus TCP upstream)
  udpretransl           (UDP upstream)
  p104send              (IEC 60870-5-104 upstream)
          |
          v
Support modules loaded per [Module] section of start.ini
  librashet.so    calculation engine
  libjevent.so    event journaling (jevent.cfg, event1 ring, 1000 slots)
  libwatch86dx.so hardware watchdog
  libterm.so      terminal abstraction
  libsystypes.so.1 shared type definitions (systypes.cc)
```

## The qmicro Engine

qmicro is the central measurement and calculation supervisor (62611 bytes,
the largest driver). It orchestrates the full RTU lifecycle:

```
LoadConfig        parse start.ini and all config files
LoadServDrv       initialise QNX IPC service layer
LoadUsoDrv        load and connect USO protocol drivers
LoadRetranslDrv   load and connect retranslation drivers
LoadKanals        load and connect channel drivers
LoadModuleDrv     load calculation module libraries
LoadDiscret       load discrete point definitions from SHM
LoadAnalog        load analog point definitions from SHM
LoadImpuls        load impulse counter definitions
LoadConst         load engineering constants
LoadFormula       load calculation formulas
LoadDout          load discrete output definitions
LoadUso           load USO device table
InitAll           initialise all subsystems
StartServer       start QNX IPC message server
StartCikl         enter main measurement cycle
```

The main cycle calls GetDiscretes, GetAnalogs, GetImpuls, TestSostUso,
applies librashet calculations, handles events, updates the SHM segments,
and calls SendEvent for upstream transmission.

Notable QMICRO functions:

```
AnalizAperture       deadband check for analog change detection
AnalizGrAperture     group deadband analysis
TimeInclude          timestamp interval check
PrepareBufDozapros   prepare re-request buffer for missing data
IsTypeBufferSupport  check if buffer type is supported by driver
CreateMappingObject  QNX shared memory object creation
ClearSharedMemory    wipe all SHM on restart
SaveSost             save current state to flash
ClearSwopDir         clear swap directory on clean start
TestRebootFile       check for unclean shutdown marker
AddTsInJournal       add TS event to journal
AddRebootJnJournal   log reboot event
```

## Startup Sequence

The sbat/preinit script brings up the QNX hardware layer. The restore binary
replays restore.scr which in turn starts the web server (Apache, confirmed
from restore.scr) and optionally the radio modem and TCP server.

restore.scr content (from filesystem dump):

```
Starts rmodem with modem.cfg if radio channel configured
Optionally starts tcpsrv (TCP server mode for serial bridging)
Starts dev-udp virtual serial device over UDP
Starts gpstime
Starts Apache web server from /flashdisk/apache
```

After preinit, qmicro reads start.ini, loads all drivers in order, and
enters the measurement cycle. The [Programms] section can run arbitrary
processes at startup (project config uses this to kill ksh for security).

## Shared Memory Segments (Full List)

From qmicro strings analysis:

```
/DefDiscrets       discrete point definitions
/SostDiscrets      discrete point runtime state
/DefAnalogs        analog point definitions
/SostAnalogs       analog point runtime state
/ExtAnalogs        extended analog buffer (used by Modbus retranslation)
/DefImpulses       impulse counter definitions
/SostImpulses      impulse counter runtime state
/DefDouts          discrete output definitions
/SostDouts         discrete output runtime state
/DefUso            USO device definitions
/SostUso           USO device runtime state
/InitUso           USO initialisation data
/DefConstants      engineering constants
/DefFormula        calculation formula table
/DefSoob           message definitions
/Config            system configuration block
/DefNameKp         KP (control point) name table
/TableReactCtl     reactor control table
/DefModbus         Modbus device definition table
/DefSmsTs          SMS TS trigger definitions
/DefSmsSoob        SMS message trigger definitions
/DefSmsTel         SMS telephone number table
/DriverChannel     channel driver registry
/DefIntervals      interval definitions
/Ex_Intervals      extended interval definitions
/VirtualUsoSemaphore  fixuso virtual USO semaphore
/VirtualUsoParametrs  fixuso virtual USO parameter block
SEM_EXT_ANALOG     semaphore for extended analog SHM
```

Persistent binary files on flash/SRAM:

```
ti32.bin       teleindication (TI) snapshot
imp32.bin      impulse counter snapshot
tu32.bin       telecontrol command log
const32.bin    constant values snapshot
sostuso32.sav  USO device state save
ts32.sav       telestatus (TS) state save
ti32.sav       TI state save
imp32.sav      impulse state save
react32.bin    reactor control state
smsts32.bin    SMS TS state
smssoob32.bin  SMS message state
smstel32.bin   SMS telephone state
modbus.bin     Modbus device state
form32.bin     formula state
inuso32.bin    USO init state
inter32.bin    interval state
ex_int32.bin   extended interval state
system32.bin   system state block
timestop.sav   timestamp at last shutdown
```

## dev-udp Virtual Serial Device

dev-udp (30584 bytes, Dec 2023) is a QNX resource manager that creates
virtual serial ports at /dev/serudp%ld backed by UDP sockets. The device
supports two LAN interfaces (net1/net2) with failover. Key parameters:

```
-u<n>         unit number (creates /dev/serudpN)
-n<path>      primary LAN interface name
-i<ip>        primary local IP
-m<path>      secondary LAN interface
-r<ip>        secondary remote IP
```

This allows KPRIS and other serial protocols to run transparently over UDP
without protocol modification, confirmed by the commented configuration in
restore.scr for two-interface KPR polling.

## File Build Timestamps

All binaries share a common build date of December 9, 2024 at 18:33 (the
main firmware release). Notable exceptions:

```
addprog/rmodem        Dec 25 2023   (older radio modem driver)
addprog/dev-udp       Dec 25 2023
addprog/gpstime       Dec 25 2023
addprog/infogps       Dec 25 2023
addprog/tcpsrv        Jul 3  2025   (newest, web server TCP bridge)
project/start.ini     Jul 3  2025   (updated with tcpsrv config)
progr/start.ini       Dec 15 2024   (active deployment config)
```
