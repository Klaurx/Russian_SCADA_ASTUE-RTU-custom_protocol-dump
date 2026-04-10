# System Architecture

## Hardware Platform

Form factor: PC/104 stack, x86 DX variant processor board.

Operating system: QNX Neutrino, confirmed by qconn presence, MsgSend and
MsgReceive IPC calls throughout, pthread_sleepon primitives, sysmgr_reboot
linkage in multiple binaries, and the devf-sram resource manager.

CPU architecture: Intel i386, all ELF binaries are 32-bit little-endian.
The floating point unit is the x87 FPU; fldz, fstp, and fadd patterns
are visible throughout libkanaldrv.so.1.

Storage: /flashdisk for primary flash, /sramdisk for SRAM, and
/flashdisk/swop for the swap area used by ring buffer persistence files.

All binaries were compiled with GCC 4.3.3 targeting QNX Neutrino with
position-independent code enabled. The calling convention is standard i386
cdecl. C++ name mangling follows the Itanium ABI which nm with the C++
demangling flag resolves cleanly. The this pointer is typically passed in
eax for thiscall variants, though some optimised forms use esi or edi as
the object register.

Hardware components identified from binary names and symbol analysis:

  gpio86dx is the PC/104 digital I/O driver for the x86 DX variant board.
    The binary implements the gpio86dx class inheriting from usodrv. It
    spawns a tuthread for telecontrol output and uses ThreadCtl for
    real-time priority control.

  amux and amux86dx implement the uso_amux class for the analog multiplexer
    on the PC/104 board. The class implements AddKodInBuffer and ReadFromBuffer
    plus an OprosAmux thread and MakeAver for averaging. It communicates with
    the ISA bus directly and prints "Amux - Error connect to ISA!!!!!" on
    failure. The PortLPT global holds the LPT base address.

  libwatch86dx.so implements a watchdog driver for the x86 DX board. It
    exports WatchDog(int) and startcikl(void*) and stores its tick function
    in the global ____Cikl. Two variants exist with slightly different sizes.

  libwatch586.so is the alternative watchdog for 586 class hardware.

  devf-sram is the QNX flash and SRAM device driver, a standard QNX binary.

  sram_format is the SRAM formatting utility.

  gpstime is the GPS receiver daemon. It opens a serial port, communicates
    with the GPS receiver using a custom framing protocol with CRC16, creates
    a POSIX shared memory segment named //gpstime, and exports timing data
    to libtmkorr.so via that segment. It calls procmgr_daemon to daemonise.
    It handles GPIO interrupts via InterruptAttachEvent and InterruptWait.
    Configuration parameters include speed, port, smhour (UTC offset), and
    gist (hysteresis). Debug output format: "Deltatime=%f %ld %ld".

  infogps is the GPS status query utility. It maps //gpstime and prints
    status including synchronisation source, request/response/error counts,
    UTC offset, and hysteresis value.

  power.cc / power binary exports PowerON() and uses ThreadCtl for IO
    access. It is a GPIO control utility for power management.

  modem_gsm implements cgsmlink for GSM modem control. It opens /dev/ser1,
    sends AT commands including AT+CFUN=1, and provides modem_on() and
    modem_off() methods. The modem_gsm utility at addprog/modem_gsm provides
    command-line control via ModemGPRS_On, ModemGPRS_Off, and ModemGPRS_View.

  rmodem is the TNC packet radio modem resource manager. It is a large binary
    implementing a full AX.25 framing stack, LZW compression (ShrinkLZW and
    UnshrinkLZW), and a QNX resource manager interface (rmodem_io_read,
    rmodem_io_write, rmodem_resmgr). It manages both a computer serial port
    and a radio serial port with independent baud rates, interrupt assignments,
    and timing parameters. It exports MCRC16 for frame integrity. The binary
    exists in two slightly different size variants.

  p104send implements the upstream IEC 60870-5-104 TCP server driver.


## Communication Paths

Primary path: TCP/IP WAN via the ctcpqnx class (TCP channel driver).
The RTU binds its local address at port 2124 and connects to the SCADA
master at port 5124. Unit address 111 is taken from the a=111 parameter
in start.ini. The asymmetric ports confirm that 5124 is the master's
well-known listening port and 2124 is the RTU's identity port.

Secondary path: UDP retranslation via the cudpqnx or cudpretr class on
port 2127 from the project configuration.

Tertiary path: VHF packet radio via rmodem and the TNC modem driver.
Physical port: /dev/ser4 at 19200 baud connected to a TNC modem. This
serves as a backup communications path for remote substations without
reliable WAN connectivity.

Quaternary path: GSM/GPRS via gsmlink and qnxgprs, using the cgsmlink
channel driver class.

Field bus: the cusotm driver runs on /dev/ser7 in the active deployment
configuration, or /dev/ser9 in the project configuration, at 115200 baud.
Memory-mapped local I/O is handled by gpio86dx.

Additional serial field protocols include ekra on RS-232 for IEC 101,
kpris on RS-232 or UDP for KPRIS substation devices, sirius_mb on RS-485
for Sirius relay IEDs, and cicpcon on RS-485 for ICP-CON devices.

Network configuration: the restore.scr script recovered from netcfg strings
reveals up to four LAN interfaces. The primary interface en0 runs via the
vortex network driver at 100 Mbit full duplex. Three additional interfaces
(/net1, /net2, /net3) use the micrel8841 and asix USB-Ethernet drivers.
Each interface runs its own inetd instance with SOCK environment variable
routing.


## Software Stack

```
GPS time source via gpstime and infogps
          |
          v
libtmkorr.so, time correction library
  Exports: ConnectToGps, IsTimeKorrectEnable
  InitModule performs a channel code calculation based on input word at 0x2a
          |
          v
libkanaldrv.so.1, RTOS link layer channel driver, kanaldrv base class
  RX:  ReadByteFromPort -> PriemPacket -> AnalizBufPriem
  TX:  MakeSrezBuf / MakeAdrBuf -> SendBuffer
  ACK: SendKvitok / ReadKvitok
  SHM: /Sem%.04u /SB%.04u /BE%.04u /BP%.04u Nreg%.04u Per%.04u

  Transport subclasses all implement kanaldrv vtable:
    ctcpqnx       TCP channel, primary WAN path
    csercom       serial channel for modem or direct serial links
    cgsmlink      GSM modem channel, subclass of csercom
    cudpqnx       UDP channel driver
    cudpretr      UDP retranslation channel
    crs485retr    RS-485 retranslation channel
    ctnc          TNC packet radio channel, full AX.25 support
          |
     +---------+----------+---------+---------+
     |         |          |         |         |
  usotm      ekra       kpris    sirius_mb  cicpcon
  cusotm     cekra      ckpris   csirius    cicpcon
  /dev/ser7  IEC 101    RS-232   RS-485     RS-485
  115200baud RS-232     or UDP   Modbus     ICP-CON
     |         |          |         |         |
     +---------+----------+---------+---------+
          |
          v
libusodrv.so.1, USO data model base class, usodrv
  I/O types:  iocDiscret, iocAnalog, iocImpuls, iocDout, iocuso
  Config SHM: /DefDiscrets, /SostDiscrets, /DefAnalogs, /SostAnalogs
              /DefImpulses, /SostImpulses, /DefDouts, /SostDouts
              /DefUso, /SostUso, /InitUso, /DefConstants, /DefFormula
  Registration: AddUserDataDiscret, AddUserDataAnalog, AddUserDataImpuls,
                AddUserDataDout, AddUserDataUso
  Additional USO device types:
    gpio86dx    PC/104 direct digital I/O
    uso_amux    analog multiplexer
    uso5600     c5600 PC/104 board
    fixuso      virtual USO in shared memory for testing
    usoimit     USO simulator, creates /ConfigImit, /ConfigTs, /ConfigTi,
                /ConfigImp, /ConfigTu shared memory segments
    crpn        RPN protocol device driver
    cmdbf       STEM-300 and CMDBF meter driver
    cserpr      SERPR retranslation driver
    ctclass     class-based RTU polling (qcet binary)
    mirclass    MIR protocol RTU polling (qmir binary)
    pusclass    PUS protocol RTU polling (qpuso binary)
    comdirect   direct COM port driver
          |
          v
librashet.so, engineering calculation engine
  Signal: TestGran, TestIn, TestMaskBit, vfabs, SQRT
  Timers: InitTimer, NextStepTimer, InitShimTimer, NextStepPulsTimer
  Relay:  ReadSostReley, GroupAnaliz, GroupAnaliz1, GroupAnaliz2
  Values: SetAnalog, SetPrValueRA, RashetAnalog, RashetDiscret
          |
          v
libservdrv.so.1, QNX IPC service driver
  Exports: ServModuleInit, LoadServDrv, ServDrvInit
  IPC:     MsgSend, MsgReceive, MsgReply via QNX message passing
          |
          v
Retranslation layer
  libretransldos.so.1  DOS-mode retranslation, retransldrv class
  libretransldrv.so.1  driver-mode retranslation, retransldrv class
  Both implement: SendKvitok, PriemPacket, ReadByteFromPort, SendBufToKanal,
    RetranslateBackupPacket, PrepareBackupRetranslateBuffer,
    GetRetranslateAdr, PrepareBufRetr
  mdbtcp / mdbtcpretr  Modbus TCP upstream retranslation
  udpretransl           UDP upstream via cudpretr or cudpqnx class
  p104send              IEC 60870-5-104 upstream server
          |
          v
Support modules loaded per the [Module] section of start.ini
  librashet.so     calculation engine
  libjevent.so.1   event journaling, jevent.cfg config, event1 ring 1000 slots
    Exports: AddTsEventJournal, AddUsoEventJournal, AddEventProtokol,
             WriteEventTs, WriteEventUsoLink, GoToNewJournal,
             RaspakIncludeEvent, RaspakExcludeEvent, MakeCrc
  libwatch86dx.so  hardware watchdog for x86 DX board
  libterm.so       terminal abstraction for local HMI
  libsystypes.so.1 shared type definitions, systypes.cc source
```


## The qmicro Engine

qmicro is the central measurement and calculation supervisor. It is not the
largest binary by file size but it orchestrates the complete RTU lifecycle.
It implements the QMICRO class with the following initialisation sequence:

```
LoadConfig          parse start.ini and all subordinate configuration files
LoadServDrv         initialise the QNX IPC service layer via ServDrvInit
LoadUsoDrv          load and connect all USO protocol drivers
LoadRetranslDrv     load and connect retranslation drivers
LoadKanals          load and connect channel drivers
LoadModuleDrv       load calculation module libraries via dlopen
LoadDiscret         load discrete point definitions from shared memory
LoadAnalog          load analog point definitions from shared memory
LoadImpuls          load impulse counter definitions
LoadConst           load engineering constants
LoadFormula         load calculation formulas
LoadDout            load discrete output definitions
LoadUso             load USO device table
InitAll             initialise all subsystems in dependency order
StartServer         start the QNX IPC message server via ChannelCreate
StartCikl           enter the main measurement cycle
```

The main cycle calls GetDiscretes, GetAnalogs, GetImpuls, and TestSostUso
on each pass, applies librashet calculations, handles events, updates all
shared memory segments, and calls SendEvent for upstream transmission via
the active channel drivers.

Selected QMICRO methods and their roles:

```
AnalizAperture         deadband check for analog change detection
AnalizGrAperture       group deadband analysis across multiple points
TimeInclude            timestamp interval membership check
PrepareBufDozapros     prepare a re-request buffer for missing data
IsTypeBufferSupport    check whether a buffer type is supported by a driver
CreateMappingObject    QNX POSIX shared memory object creation
ClearSharedMemory      wipe all shared memory segments on clean restart
SaveSost               save current state to flash for power-fail recovery
ClearSwopDir           clear the swap directory on clean start
TestRebootFile         check for an unclean shutdown marker file
AddTsInJournal         add a telestatus change event to the event journal
AddRebootJnJournal     log a reboot event with cause code
FORMULA                apply a formula definition to a set of points
ANALIZ_ANALOG_KOD      analyse analog code for fault conditions
ANALIZ_DREBEZG         contact bounce analysis for discrete inputs
SCHETCHIK_FORMULA      impulse counter formula evaluation
AnalizAperture         individual analog deadband evaluation
AnalizGrAperture       group analog deadband evaluation
AnalizPeriodForAnalog  period analysis for analog transmission scheduling
ANALOG_USO             drive analog polling for a specific USO device
DISCRET_USO            drive discrete polling for a specific USO device
IMPULS_USO             drive impulse counter polling for a specific USO device
ANALIZ_IMPULS          process accumulated impulse counter deltas
```


## The Aqalpha Binary

Aqalpha (181440 bytes, the largest binary in the firmware set) has not yet
been fully analysed. Its name and symbol structure suggest it is the primary
application supervisor built on the aclass base class, distinct from the
QMICRO calculation engine. Version strings in its strings section include
"Aqalpha ver 3.7 arm" and "[VERINFO] Aqlpha ARM ver 3.6". Note that the
second string contains a typographical error in the original firmware source:
Aqlpha instead of Aqalpha. This appears to be an old bug in the version
reporting function that persisted through multiple firmware releases.

The binary contains extensive protocol handling code including ConvLocToSer,
ConvLocToSerKan, ConvLocTimeSyst, SEND_ZAPROS, NEXT_PRIBOR, and cryptographic
functions (des, Permutation, Decode). The pusclass base used by qpuso appears
related to the Aqalpha class hierarchy. The binary also contains setimptimer,
newtimer, and the full fimp/fkor timing infrastructure.


## The sqmicro Watchdog

sqmicro (source file sqmicro.cc) is a secondary watchdog utility. Its
primary function chmod_all() recursively sets permissions on the flash
filesystem. It also calls sysmgr_reboot for clean restart. Its strings section
shows it spawns updatepo and qmicro as child processes, confirming its role
as a supervisor that ensures the main measurement engine is running with
correct file permissions.


## The fixuso Virtual USO

fixuso creates two named POSIX shared memory objects:

  /VirtualUsoSemaphore
  /VirtualUsoParametrs

This implements a virtual (simulated) USO device entirely in shared memory,
used for testing or for bridging non-serial data sources into the USO data
model without requiring physical hardware. The fixuso class inherits from
usodrv and implements GetDiscret, GetAnalog, GetImpuls, SetDout, Working,
and InitUso.


## Startup Sequence

The sbat/preinit script brings up the QNX hardware abstraction layer. The
restore binary replays restore.scr which configures networking and optionally
starts support services.

The restore.scr content recovered from netcfg strings reveals the following
startup sequence:

  Network initialisation: starts io-pkt-v4-hc for each LAN interface. The
    primary interface uses the vortex driver at 100 Mbit full duplex. The
    second interface uses the micrel8841 driver. Third and fourth interfaces
    use the asix USB-Ethernet driver for bus 1 devices 1 and 2.

  inetd is started for each interface using SOCK environment variable routing.

  gpstime is started from /sd0/flashdisk/addprog/gpstime.

  Apache web server is started from /flashdisk/apache/bin/apachectl. The
    access permissions are set with chmod 0777 before start.

  Radio modem: optionally started if a radio channel is configured:
    /sd0/flashdisk/addprog/rmodem with modem.cfg argument.

  dev-udp: optionally started for two-interface KPRIS polling. Commented
    out in the recovered configuration but shows the invocation format:
    dev-udp -u1 -n/net1 -i[REDACTED] -m/net2 -r[REDACTED]

  tcpsrv: optionally started for TCP-to-serial bridging mode.

After restore.scr completes, qmicro reads start.ini, loads all drivers in
the order specified in the [Module], [Uso], and [Kanal] sections, and enters
the measurement cycle. The [Programms] section can run arbitrary processes
at startup; the project configuration uses this to terminate ksh for security.


## Shared Memory Segments

From qmicro strings analysis, the complete set of POSIX shared memory
segments used at runtime:

```
/DefDiscrets           discrete point definitions
/SostDiscrets          discrete point runtime state
/DefAnalogs            analog point definitions
/SostAnalogs           analog point runtime state
/ExtAnalogs            extended analog buffer for Modbus retranslation
/DefImpulses           impulse counter definitions
/SostImpulses          impulse counter runtime state
/DefDouts              discrete output definitions
/SostDouts             discrete output runtime state
/DefUso                USO device definitions
/SostUso               USO device runtime state
/InitUso               USO initialisation data
/DefConstants          engineering constants
/DefFormula            calculation formula table
/DefSoob               message definitions
/Config                system configuration block
/DefNameKp             control point name table
/TableReactCtl         reactor control table
/DefModbus             Modbus device definition table
/DefSmsTs              SMS telestatus trigger definitions
/DefSmsSoob            SMS message trigger definitions
/DefSmsTel             SMS telephone number table
/DriverChannel         channel driver registry
/DefIntervals          interval definitions
/Ex_Intervals          extended interval definitions
/VirtualUsoSemaphore   fixuso virtual USO semaphore
/VirtualUsoParametrs   fixuso virtual USO parameter block
SEM_EXT_ANALOG         semaphore for the extended analog shared memory
```

Persistent binary state files on flash and SRAM:

```
timestop.sav       timestamp at last clean shutdown
ti32.bin           teleindication snapshot
imp32.bin          impulse counter snapshot
tu32.bin           telecontrol command log
const32.bin        engineering constants snapshot
sostuso32.sav      USO device state save
ts32.sav           telestatus state save
ti32.sav           teleindication state save
imp32.sav          impulse state save
react32.bin        reactor control state
smsts32.bin        SMS telestatus state
smssoob32.bin      SMS message state
smstel32.bin       SMS telephone state
modbus.bin         Modbus device state
form32.bin         formula state
inuso32.bin        USO initialisation state
inter32.bin        interval state
ex_int32.bin       extended interval state
system32.bin       system state block
```


## The dev-udp Virtual Serial Device

dev-udp (30584 bytes, build date December 2023) is a QNX resource manager
that creates virtual serial ports at /dev/serudpN backed by UDP sockets.
It implements cudpqnx for the primary channel and supports two LAN interfaces
with failover. Key command-line parameters:

```
u<n>      unit number, creates /dev/serudpN
n<path>   primary LAN interface name
i<ip>     primary local IP address
m<path>   secondary LAN interface name
r<ip>     secondary remote IP address
```

This allows KPRIS and other serial-framed protocols to run transparently
over UDP without any modification to the protocol drivers themselves. The
commented configuration in restore.scr demonstrates two-interface operation
for redundant KPRIS polling.


## File Build Timestamps

```
Binary                   Build date            Notes
progr/usotm              December 9, 2024      main firmware release
progr/kpris              December 9, 2024      main firmware release
progr/ekra               December 9, 2024      main firmware release
progr/qmicro             December 9, 2024      main firmware release
progr/sirius_mb          December 9, 2024      main firmware release
libkanaldrv.so.1         December 9, 2024      main firmware release
libusodrv.so.1           December 9, 2024      main firmware release
libservdrv.so.1          December 9, 2024      main firmware release
addprog/rmodem           December 25, 2023     older radio modem driver
addprog/rmodem_wrk       December 25, 2023     radio modem worker variant
addprog/dev-udp          December 25, 2023
addprog/gpstime          December 25, 2023
addprog/infogps          December 25, 2023
addprog/tcpsrv           July 3, 2025          newest file in the set
project/start.ini        July 3, 2025          updated with tcpsrv config
progr/start.ini          December 15, 2024     active deployment config
```
