# System Architecture

## Hardware Platform

Form factor: PC/104 stack, x86 DX variant processor board.

Operating system: QNX Neutrino, confirmed by qconn presence, MsgSend and
MsgReceive IPC calls throughout, pthread_sleepon primitives, sysmgr_reboot
linkage in multiple binaries, and the devf-sram resource manager.

CPU architecture: Intel i386, all ELF binaries are 32-bit little-endian.
The floating point unit is the x87 FPU; fldz, fstp, and fadd patterns
are visible throughout libkanaldrv.so.1 and the altclass qalfat binary
where ftek18 reads float scaling coefficients from the USA struct.

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

  amux implements uso_amux for the plain analog multiplexer board. It
    exports AddKodInBuffer, ReadFromBuffer, MakeAver for averaging across
    samples, and OprosAmux as the polling thread. The PortLPT global holds
    the LPT base address used to address the ISA bus.

  amux86dx implements the same uso_amux class for the x86 DX variant board.
    It adds SetAdr(unsigned char) which is absent from the plain amux binary,
    confirming that the x86 DX board uses an addressable multiplexer scheme
    where each channel must be explicitly selected before reading. Both
    variants use PortLPT and ConnectToUso from usodrv.

  c5600 implements uso5600 for the c5600 PC/104 ISA digital I/O board.
    It uses atomic_add and atomic_sub for thread-safe counter operations and
    calls pthread_attr_setinheritsched and pthread_attr_setschedparam to
    configure real-time scheduling for its two threads: Opros5600 is the
    primary polling thread and Opr is a secondary processing thread. It
    calls ConnectToDiscrets, ConnectToImpuls, DisconnectFromDiscrets, and
    DisconnectFromImpuls from usodrv, confirming it writes directly to
    shared memory segments without any serial protocol intermediary. The
    clock_gettime call is used for high-resolution timestamp capture.

  libwatch86dx.so implements a watchdog driver for the x86 DX board. It
    exports WatchDog(int) and startcikl(void*) and stores its tick function
    in the global ____Cikl.

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

  power exports PowerON() and uses ThreadCtl for IO access. It is a GPIO
    control utility for power management of substation power relays. The
    main function calls PowerON and exits immediately. This is a one-shot
    utility, not a daemon.

  modem_gsm implements cgsmlink for GSM modem control. It opens /dev/ser1,
    sends AT commands including AT+CFUN=1 (full modem power on), and
    provides modem_on() and modem_off() methods.

  rmodem is the TNC packet radio modem resource manager. It is a large binary
    implementing a full AX.25 framing stack, LZW compression (ShrinkLZW and
    UnshrinkLZW), and a QNX resource manager interface (rmodem_io_read,
    rmodem_io_write, rmodem_resmgr). It manages both a computer serial port
    and a radio serial port with independent baud rates, interrupt assignments,
    and timing parameters. It exports MCRC16 for frame integrity. The binary
    exists in two slightly different size variants. The radio port is
    configured as /dev/ser4 at 19200 baud in the active modem.cfg.

  p104send implements the upstream IEC 60870-5-104 TCP server driver as
    the cp104send class subclassing kanaldrv. Full ASDU construction is
    implemented for all supported data types. The srez to IEC 104 conversion
    path is CreateSrezForRemoteKp. Time handling uses ConvertToSystemTime56
    for the 56-bit CP56Time2a timestamp format required by IEC 104.

  comdirect implements the comdirect class as a raw serial COM port driver.
    It exposes direct serial access through the USO interface without any
    protocol framing. The port path in its strings section uses a Windows-
    style backslash prefix "\dev\ser1" rather than the Unix-style "/dev/ser1"
    found in all other drivers, which is a notable anomaly suggesting this
    driver was partially ported from a non-QNX environment. Parameters:
    Port, Speed, Stopbits, Parity, TimeByte (inter-byte timeout in ms),
    TimeOut (overall receive timeout).

  pdebug is the QNX GDB-protocol remote debugger daemon. It is a standard
    QNX Neutrino system binary, build date May 20, 2009, version 6.4.1.
    It provides process-level debugging over TCP/IP or serial connection.
    Its presence in the firmware confirms the system retained full debugging
    capability in the deployed build.

  flashctl is the QNX flash filesystem control utility. It is a standard
    QNX binary providing erase, format, mount, unmount, lock, unlock, and
    compression control for the f3s flash filesystem. Build date May 20, 2009,
    version 6.4.1.

  restore is the script executor for restore.scr. It reads the script file,
    parses it into an array of command lines via ParseScr, then executes each
    line via spawn and system calls. The binary accepts the script path as a
    command line argument. The restore.scr path /sd0/flashdisk/addprog/rmodem
    appears in the script content itself.


## Communication Paths

Primary path: TCP/IP WAN via the ctcpqnx class (TCP channel driver).
The RTU binds its local address at port 2124 and connects to the SCADA
master at port 5124. Unit address 111 is taken from the a=111 parameter
in progr/start.ini. The asymmetric ports confirm that 5124 is the master's
well-known listening port and 2124 is the RTU's identity port.

Secondary path: UDP retranslation via the cudpretr class on port 2127
from the project configuration, using the udpretransl binary.

Tertiary path: VHF packet radio via rmodem and the ctnc TNC modem driver.
Physical port: /dev/ser4 at 19200 baud connected to a TNC modem as
configured in modem.cfg. This serves as a backup communications path for
remote substations without reliable WAN connectivity.

Quaternary path: GSM/GPRS via gsmlink and qnxgprs, using the cgsmlink
channel driver class subclassing csercom.

Alarm path: tcparm uses ctcpqnx via libkanalarm.so.1 rather than
libkanaldrv.so.1, providing a distinct channel with alarm-specific timing
or acknowledgment semantics separate from the main data channel.

Field bus: the cusotm driver runs on /dev/ser7 in the active deployment
configuration (progr/start.ini), or /dev/ser9 in the project configuration,
at 115200 baud. Memory-mapped local I/O is handled by gpio86dx directly
via the PC/104 ISA bus.

Additional serial field protocols include ekra on RS-232 for IEC 101,
kpris on RS-232 or UDP for KPRIS substation devices, sirius_mb on RS-485
for Sirius relay IEDs, and cicpcon on RS-485 for ICP-CON devices.

Network configuration: the restore.scr script recovered from the restore
binary and netcfg strings reveals up to four LAN interfaces. The primary
interface en0 runs via the vortex network driver at 100 Mbit full duplex.
Three additional interfaces (/net1, /net2, /net3) use the micrel8841 and
asix USB-Ethernet drivers. Each interface runs its own inetd instance with
SOCK environment variable routing.


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
    ctcpqnx       TCP channel, primary WAN path (progr/tcpqnx)
    ctcpqnx       TCP alarm channel, alarm path (progr/tcparm via
                    libkanalarm.so.1 instead of libkanaldrv.so.1)
    csercom       serial channel for modem or direct serial links (sercom)
    cgsmlink      GSM modem channel, subclass of csercom (gsmlink)
    cudpqnx       UDP channel driver (udpqnx)
    cudpretr      UDP retranslation channel (udpretransl)
    crs485retr    RS-485 retranslation channel (rs485retransl via
                    libretransldrv.so.1, rs485dosretransl via
                    libretransldos.so.1)
    ctnc          TNC packet radio channel, full AX.25 support (tnc)
    cp104send     IEC 60870-5-104 upstream server (p104send)
          |
     +---------+----------+---------+---------+---------+--------+
     |         |          |         |         |         |        |
  usotm      usotmj     usom      usom2     ekra     kpris    ...
  cusotm     cusotmj    cusom     cusom     cekra    ckpris
  /dev/ser7  SOE ext    RS-232    RS-232    IEC101   RS-232
  115200baud change     USOM bus  USOM+ext  RS-232   or UDP
             polling
     |
     +---------+----------+---------+---------+
     |         |          |         |         |
  sirius_mb  cicpcon     rpn      stem300  mdbf/mdbtcp/...
  csirius    cicpcon     crpn     cmdbf    cmdbf variants
  RS-485     RS-485      RS-232   RS-232   serial or TCP
  Modbus     ICP-CON     RPN      STEM300  Generic Modbus
     |
     +---------+----------+---------+---------+---------+
     |         |          |         |         |         |
  qalfat    qcet        qmir     qpuso    qpty    qptym
  altclass  ctclass     mirclass pusclass pclass  ptmclass
  RS-232    RS-232      RS-232   RS-232   RS-232  RS-232
  DES crypt class poll  MIR var  PUS var  PTY var PTM var
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
    gpio86dx    PC/104 direct digital I/O (x86 DX variant)
    uso_amux    analog multiplexer (plain and x86DX variants)
    uso5600     c5600 PC/104 ISA board driver
    fixuso      virtual USO in shared memory for testing
    usoimit     USO simulator
    comdirect   direct COM port driver
    crpn        RPN protocol device driver
    cmdbf       STEM-300 and CMDBF meter driver (multiple variants)
    cserpr      SERPR retranslation driver
    ctclass     class-based RTU polling (qcet binary)
    mirclass    MIR protocol RTU polling (qmir binary)
    pusclass    PUS protocol RTU polling (qpuso binary)
    pclass      PTY protocol RTU polling (qpty binary)
    ptmclass    PTM protocol RTU polling (qptym binary)
    altclass    DES-encrypted altclass protocol (qalfat binary)
    aclass      primary supervisor class (Aqalpha binary)
          |
          v
librashet.so, engineering calculation engine
  Signal:   TestGran, TestIn, TestMaskBit, TestBit, TestChange, vfabs, SQRT
  Analysis: TestRash, TestRash1, TestTime, UpdateFlags, GetValueWithFirst
  Timers:   InitTimer, NextStepTimer, InitShimTimer, NextStepPulsTimer,
            InitPulsTimer, NextStepShimTimer, ClearShimTimer,
            InitTimerZ, NextStepTimerZ, GetDelayTimer, GetDlitTimer,
            KorrectTime, NetxStepOtchet
  Relay:    ReadSostReley, GroupAnaliz, GroupAnaliz1, GroupAnaliz2
  Values:   SetAnalog, SetPrValueRA, SetValueRA, RashetAnalog, RashetDiscret,
            SbrosAnalog, ReconfigAnalog, ReconfigDiscret, ReconfigDout,
            ReconfigImpuls
  Internal: _SetAnalogValue, _SetConstantValue, _SetDiscretValue,
            _SetImpulsValue (direct shared memory write operations)
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
             RaspakIncludeEvent, RaspakExcludeEvent, MakeCrc,
             AddEventDiscret, AddEventUsoLink, AddRecordEventProtokol,
             ReadConfig, OpenNewJournal, CloseJournal, TestJournal,
             RaspakTs, RaspakUso, RaspakKey, Read_Key, Read_Key_Value
  libwatch86dx.so  hardware watchdog for x86 DX board
  libterm.so       terminal abstraction for local HMI
    Panel hardware: /dev/term_kbd (keyboard), /dev/term_lcd (display)
    Exports: InitPanel, InitLED, CursorHome, CursorOff, CursorOn,
             CursorUnderline, MoveCursor, PutString, PutNewString,
             SendCommandLED, ReadAnsw, ReadData, Kbd (thread), Term (thread)
    Menu system: MainMenu, PodMenu0 through PodMenu4 as static trees
    Reads start.ini path from DisplayName global
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
AnalizPeriodForAnalog  period analysis for analog transmission scheduling
ANALOG_USO             drive analog polling for a specific USO device
DISCRET_USO            drive discrete polling for a specific USO device
IMPULS_USO             drive impulse counter polling for a specific USO device
ANALIZ_IMPULS          process accumulated impulse counter deltas
```


## The Aqalpha Binary and aclass

Aqalpha (181440 bytes, the largest binary in the firmware set) is the primary
application supervisor. Its class name is aclass, not pusclass or any variant
of the field bus driver hierarchy. The aclass constructor takes
SUPPORTED_USO_AND_BUF_TYPES rather than the unsigned short used by the field
bus driver constructors, which confirms it operates at a higher level in the
firmware hierarchy than the USO device drivers.

The source file name recovered from the strings section is Aqalpha.cc.

Version strings: "Aqalpha ver 3.7 arm" and "[VERINFO] Aqlpha ARM ver 3.6".
The second string contains a typographical error in the original firmware
source: Aqlpha instead of Aqalpha. This appears to be an old bug in the
version reporting function that persisted through multiple firmware releases.

The launch format string "Aqalpha ver 3.7 arm p/dev/ser[N] s[speed] d[d] ...."
confirms Aqalpha accepts command line arguments for serial port path, baud
rate, and a mode discriminator byte.

Aqalpha links against libsocket.so.2 and uses accept, bind, listen, recv,
send, socket, setsockopt, and shutdown, confirming it hosts a TCP server
internally. This is distinct from the upstream RTOS channel handled by
libkanaldrv.so.1 and represents a separate communication endpoint.

The aclass method table confirmed from nm:

```
aclass::aclass(SUPPORTED_USO_AND_BUF_TYPES*)   constructor
aclass::~aclass()                               destructor (three variants)
aclass::AddUserDataAnalog(...)                  register analog point
aclass::AddUserDataUso(...)                     register USO device
aclass::fdost(unsigned short, MSG_RETURN_ANALOG*)
  analog data fetch for a single channel by index
aclass::fener(unsigned short, unsigned int, double)
  energy calculation function, takes channel index, counter value, and
  a double precision engineering coefficient
aclass::fjur(char*)
  journal output function, takes a file path string
aclass::fkrt(USA*)
  device characterization function, takes a direct USA struct pointer
  rather than going through GetUsomUSA indirection
aclass::fsost(unsigned short)
  device state query by channel index
aclass::fsq(TIME_SERVER_KANAL)
  timestamp sequencing function, takes a full BCD timestamp struct by value
aclass::fzUso()
  USO device zero-reset or initialization function
aclass::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
aclass::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
aclass::GetUsomUSA(iocuso*)
  device state resolver, returns pointer to internal USA struct
aclass::KorAdr()
  address correction and timing synchronization before each poll cycle
aclass::MakeUsoSpecialBuf(MSG_SPECIAL_BUF*, MSG_SPECIAL_BUF**)
  special buffer routing, same pattern as other RTU class drivers
```

The crypto functions resident in Aqalpha are des, Permutation, SBoxes, Xor,
bytestobit, bitstobytes, encrypt, and additionally encrypt17 which takes
three char pointer arguments. The encrypt17 signature differs from the
qalfat encrypt function, suggesting a distinct 17-round DES variant or a
block cipher operating on 17-byte input blocks. fencrypt wraps encrypt for
frame-level application. The full DES key schedule is stored in the keys
array at 0x80785a0.

The binary also carries ConvLocTimeSyst, ConvLocToSer, ConvLocToSerKan for
BCD timestamp conversion, plus NEXT_PRIBOR and setimptimer confirming the
supervisor pattern shared with other RTU class binaries.

The USA struct is accessed through aclass::GetUsomUSA(iocuso*) using the
same single pointer dereference at iocuso+0x20 confirmed from altclass
disassembly. The USA struct layout derived from all aclass and altclass call
site analysis is documented in src/structs/altclass_types.h.

Aqalpha calls usodrv::GetSostBuffer, usodrv::SendNewEvent,
usodrv::GetItemValueByte, and usodrv::GetItemValueDWord, confirming it
both reads and writes the shared memory I/O segments maintained by qmicro.

The address table at 0x80547a0 maps device slot indices to USA struct
pointers. The device count at 0x8052e08 is checked before iterating.


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


## The usoimit Simulator

usoimit implements the usoimit class which creates five shared memory segments:

  /ConfigImit    simulation configuration data
  /ConfigTs      telestatus simulation table
  /ConfigTi      teleindication simulation table
  /ConfigImp     impulse counter simulation table
  /ConfigTu      telecontrol simulation table

This simulator replays pre-configured I/O patterns for testing the qmicro
engine without requiring physical hardware. It can simulate complete I/O
cycles including event generation and counter accumulation.


## Startup Sequence

The sbat/preinit script brings up the QNX hardware abstraction layer. The
restore binary replays restore.scr which configures networking and optionally
starts support services.

The restore.scr content fully recovered from the restore binary strings
reveals the following startup sequence:

  Network initialisation: starts io-pkt-v4-hc for each LAN interface. The
    primary interface uses the vortex driver at 100 Mbit full duplex. The
    second interface uses the micrel8841 driver. Third and fourth interfaces
    use the asix USB-Ethernet driver for bus 1 devices 1 and 2.

  inetd is started for each interface using SOCK environment variable routing.

  gpstime is started from /sd0/flashdisk/addprog/gpstime.

  Apache web server: the script checks for and removes any stale httpd.pid
    file, then calls random -t and waitfor /dev/random 10 before sleeping 3
    seconds to ensure entropy is available. Filesystem permissions are set
    with chmod 0777 on /sd0/flashdisk and its subdirectories. The CGI backend
    /flashdisk/apache/cgi-bin/websrv is launched as a daemon and then
    apachectl start is called.

  Radio modem: optionally started if a radio channel is configured:
    /sd0/flashdisk/addprog/rmodem with modem.cfg argument. Commented out
    in the recovered configuration.

  dev-udp: optionally started for two-interface KPRIS polling. Commented
    out in the recovered configuration but shows the invocation format:
    dev-udp -u1 -n/net1 -i[REDACTED] -m/net2 -r[REDACTED]
    A second instance with -u2 is also commented out.

  tcpsrv: optionally started for TCP-to-serial bridging mode. Also commented
    out with the invocation format:
    tcpsrv i[REDACTED],2101 p/dev/ser7 s115200

  A commented entry shows devc-ser8250 invocation for activating COM9 using
    base address 0x10 with IRQ 5:
    devc-ser8250 -b115200 -F -S -u9 0x10,0x5

After restore.scr completes, qmicro reads start.ini, loads all drivers in
the order specified in the [Module], [Uso], and [Kanal] sections, and enters
the measurement cycle. The [Programms] section in the project configuration
runs /proc/boot/slay -f ksh to terminate ksh for security hardening.


## Active Deployment Configuration (progr/start.ini)

The progr/start.ini file is the configuration loaded by qmicro in the active
deployed firmware. Its full content recovered from binary strings:

```
[Disks]
FileStartProtokol=/flashdisk/start
FlashDiskPath=/flashdisk/bin/
StaticDiskPath=/sramdisk
SwopDiskPath=/flashdisk/swop

[Uso]
Uso=usotm p/dev/ser7 s115200
Uso=gpio86dx

[Kanals]
Kanal=tcpqnx a111 i[REDACTED],2124,[REDACTED],5124

[Module]
Module=librashet.so
Module=libjevent.so o/flashdisk/jevent.cfg f/flashdisk/event1 s1000
Module=libwatch86dx.so
```

The libjevent.so module parameter s1000 sets the ring buffer to 1000 event
slots. The o parameter sets the journal configuration file path. The f
parameter sets the journal output file path prefix.


## Project Configuration (project/start.ini)

The project/start.ini file represents an alternate deployment configuration,
updated July 3, 2025 alongside the tcpsrv binary. Its full content:

```
[Disks]
FileStartProtokol=/flashdisk/start
FlashDiskPath=/flashdisk/bin/
StaticDiskPath=/sramdisk
SwopDiskPath=/flashdisk/swop

[Uso]
Uso=usotm p/dev/ser9 s115200

[Kanals]
Kanal=tcpqnx a5 i[REDACTED],2124,[REDACTED],2124

[Module]
Module=librashet.so
Module=libjevent.so o/flashdisk/jevent.cfg f/flashdisk/event1
Module=libwatch86dx.so
Module=libterm.so

[Retransl]
_Retransl=udpretransl p2127

[Programms]
_Programm=/proc/boot/slay -f ksh
```

Notable differences from the active deployment: the field bus port is
/dev/ser9 rather than /dev/ser7, the unit address is 5 rather than 111,
the TCP ports are symmetric (both 2124) rather than asymmetric, libterm.so
is loaded for local HMI panel support, a UDP retranslation path is active
on port 2127, and the ksh process is killed on startup for security.


## modem.cfg (radio modem configuration)

The modem.cfg file configures the rmodem TNC packet radio modem driver.
The file is written in Russian with KOI8-R encoding in the comment lines.
All operational parameters are in ASCII. The full content:

```
Radio Port=/dev/ser4
Computer port = com2
radiobaud=19200
COMPUTER baud=38400
Radio COM IRQ=4
Computer COM IRQ=3
Control=4
num buf=max
num buf rep=50
num msg=0
x=1
y=4
a=1
n=2
o=4
r=1
e=1
w=10
p=64
v=0
f=250
t=1
t2=100
t3=18000
TimeWaitRadio=3
hwd=4
inversDTR=0
inversGOT=0
maxTimeRadioGot=30000
maxTimeRadioMute=90000
maxTimeRadioDumb=30000
durationReset=150

[Disks]
FileStartProtokol=/flashdisk/start
FlashDiskPath=/flashdisk/bin/
StaticDiskPath=/sramdisk
SwopDiskPath=/flashdisk/swop
```

Parameter meanings decoded from Russian comments:
  Control=4 enables special mode 4, setting CI/EI or CR flags in HOST mode
  num buf=max sets the buffer count to the maximum supported value
  num buf rep=50 sets 50 retransmit buffers
  x=1 enables PTT wait before transmission
  y=4 sets the number of connection attempts to 4
  a=1 enables address line mode
  n=2 sets 2 connection retries minimum
  o=4 sets 4 maximum transmit attempts
  r=1 enables repeat mode
  e=1 enables echo output
  w=10 sets the slot timer in tenths of seconds
  p=64 sets the pause access value
  v=0 disables connect confirmation
  f=250 sets the response timeout, minimum value
  t=1 sets the delay for next repeater
  t2=100 sets the time to wait for the next packet frame in milliseconds
  t3=18000 sets the timeout for radio check
  TimeWaitRadio=3 sets the time in seconds to wait for a radio symbol
  hwd=4 selects LPT port 4 for hardware watchdog, type 586
  inversDTR=0 normal DTR signal polarity
  inversGOT=0 normal GOT signal polarity
  maxTimeRadioGot=30000 maximum time in ms to wait for GOT during transmission
  maxTimeRadioMute=90000 maximum silence time on radio channel (15 minutes)
  maxTimeRadioDumb=30000 maximum time for radio receiver inactivity (5 minutes)
  durationReset=150 reset pulse duration in milliseconds


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

Persistent state files used by the RTU class polling drivers:

```
/ALP.SAV    used by qmir (mirclass) for session state persistence
/PTY.SAV    used by qpty (pclass) and qpuso (pusclass) for session state
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
over UDP without any modification to the protocol drivers themselves.


## File Build Timestamps

```
Binary                   Build date            Notes
progr/usotm              December 9, 2024      main firmware release
progr/usotmj             December 9, 2024      main firmware release
progr/usom               December 9, 2024      main firmware release
progr/usom2              December 9, 2024      main firmware release
progr/kpris              December 9, 2024      main firmware release
progr/ekra               December 9, 2024      main firmware release
progr/qmicro             December 9, 2024      main firmware release
progr/sirius_mb          December 9, 2024      main firmware release
progr/qalfat             December 9, 2024      main firmware release
progr/qcet               December 9, 2024      main firmware release
progr/qmir               December 9, 2024      main firmware release
progr/qpuso              December 9, 2024      main firmware release
progr/qpty               December 9, 2024      main firmware release
progr/qptym              December 9, 2024      main firmware release
progr/Aqalpha            December 9, 2024      main firmware release
progr/cicpcon            December 9, 2024      main firmware release
progr/rpn                December 9, 2024      main firmware release
progr/stem300            December 9, 2024      main firmware release
progr/mdbf               December 9, 2024      main firmware release
progr/mdbf80             December 9, 2024      main firmware release
progr/mdbfo              December 9, 2024      main firmware release
progr/mdbtcp             December 9, 2024      main firmware release
progr/c5600              December 9, 2024      main firmware release
progr/amux               December 9, 2024      main firmware release
progr/amux86dx           December 9, 2024      main firmware release
progr/comdirect          December 9, 2024      main firmware release
progr/tcpqnx             December 9, 2024      main firmware release
progr/tcparm             December 9, 2024      main firmware release
progr/udpqnx             December 9, 2024      main firmware release
progr/sercom             December 9, 2024      main firmware release
progr/gsmlink            December 9, 2024      main firmware release
progr/tnc                December 9, 2024      main firmware release
progr/rs485retransl      December 9, 2024      main firmware release
progr/rs485dosretransl   December 9, 2024      main firmware release
progr/p104send           December 9, 2024      main firmware release
progr/sqmicro            December 9, 2024      main firmware release
progr/fixuso             December 9, 2024      main firmware release
progr/usoimit            December 9, 2024      main firmware release
libkanaldrv.so.1         December 9, 2024      main firmware release
libusodrv.so.1           December 9, 2024      main firmware release
libservdrv.so.1          December 9, 2024      main firmware release
librashet.so             December 9, 2024      main firmware release
libjevent.so             December 9, 2024      main firmware release
libterm.so               December 9, 2024      main firmware release
addprog/rmodem           December 25, 2023     older radio modem driver
addprog/rmodem_wrk       December 25, 2023     radio modem worker variant
addprog/dev-udp          December 25, 2023
addprog/gpstime          December 25, 2023
addprog/infogps          December 25, 2023
addprog/tcpsrv           July 3, 2025          newest file in the set
project/start.ini        July 3, 2025          updated with tcpsrv config
progr/start.ini          December 15, 2024     active deployment config
```
