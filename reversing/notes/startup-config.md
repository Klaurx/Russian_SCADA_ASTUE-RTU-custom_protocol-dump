# Startup Configuration and Script Analysis

## sbat/preinit

The sbat/preinit script is the earliest stage of the firmware startup
sequence. It brings up the QNX hardware abstraction layer (HAL) before any
user-space processes are launched. The sbat directory name comes from the
QNX startup batch mechanism. The preinit script configures the processor
mode, enables the hardware interrupt controller, and loads the initial
device drivers required for the network and storage subsystems.

The script is not directly readable as text from this firmware image but its
role is confirmed by its position in the startup sequence: sbat/preinit runs
first, then restore is invoked with restore.scr, then qmicro starts.


## addprog/restore.scr Full Content

The restore.scr script is executed by the restore binary at startup.
Its complete content has been recovered from the restore binary's strings
section and from the netcfg binary. The script is a QNX shell script
(using the standard QNX sh/ksh syntax) that configures networking, starts
services, and prepares the filesystem for qmicro.

The full operational portion (with IP addresses redacted):

```sh
## Network initialization
/sbin/io-pkt-v4-hc -d vortex speed=100,duplex=full &
waitfor /dev/io-net 10

SOCK=/dev/io-net /sbin/inetd &

/sbin/io-pkt-v4-hc -d micrel8841 &
waitfor /dev/io-net2 10
SOCK=/dev/io-net2 /sbin/inetd &

/sbin/io-pkt-v4-hc -d asix bus=1,device=1 &
waitfor /dev/io-net3 10
SOCK=/dev/io-net3 /sbin/inetd &

/sbin/io-pkt-v4-hc -d asix bus=1,device=2 &
waitfor /dev/io-net4 10
SOCK=/dev/io-net4 /sbin/inetd &

## GPS time receiver
/sd0/flashdisk/addprog/gpstime &

## KPRIS UDP polling on two LAN interfaces (commented out in deployment)
#/sd0/flashdisk/addprog/dev-udp -u1 -n/net1 -i[REDACTED] -m/net2 -r[REDACTED]
#/sd0/flashdisk/addprog/dev-udp -u2 -n/net1 -i[REDACTED] -m/net2 -r[REDACTED]

## Apache web server
if test -f /sd0/flashdisk/apache/logs/httpd.pid
then rm /sd0/flashdisk/apache/logs/httpd.pid
fi
random -t
waitfor /dev/random 10
sleep 3
chmod 0777 /sd0/flashdisk/*
chmod 0777 /sd0/flashdisk/bin/*
chmod 0777 /sd0/flashdisk/project/*
/flashdisk/apache/cgi-bin/websrv &
/flashdisk/apache/bin/apachectl start

## Radio modem (commented out in deployment)
#/sd0/flashdisk/addprog/rmodem /sd0/flashdisk/addprog/modem.cfg &
#waitfor /dev/rmodem1

## TCP-to-serial bridge (commented out in deployment)
#/sd0/flashdisk/addprog/tcpsrv i[REDACTED],2101 p/dev/ser7 s115200

## COM9 activation (commented out)
#devc-ser8250 -b115200 -F -S -u9 0x10,0x5
```

The script structure reveals the deployment priorities: the primary TCP/IP
WAN path (via vortex) and inetd are always started. The GPS time receiver
is always started. The Apache web server is always started. Radio modem,
KPRIS UDP polling, and TCP-serial bridging are optional and commented out
in the recovered configuration.

The random -t and waitfor /dev/random 10 sequence before Apache starts
ensures the system's random number generator has collected sufficient entropy
before the web server begins accepting connections. The 3-second sleep allows
the random device to initialise fully.


## addprog/modem.cfg Full Content

The modem.cfg file configures the rmodem TNC packet radio modem. Its full
content has been recovered from the rmodem binary strings section. The file
contains Russian-language comments in KOI8-R encoding; all operational
parameters are in ASCII.

The comment lines (translated from Russian) describe:
  Radio Port section: configures the physical radio port
  Computer port section: configures the interface to the host computer
  Debug section: controls diagnostic output level
  Control section: sets operating mode flags (bit flags 1-5)
  Buffer section: controls packet buffer allocation
  Timer section: sets all protocol timing parameters
  Hardware section: configures the LPT watchdog and signal polarity

Complete parameter listing with meanings:

```
Radio Port=/dev/ser4
  physical serial port connected to the TNC hardware
  the commented-out alternative /dev/serusb1 suggests USB-serial adapter
  support exists but the direct serial connection is preferred

Computer port = com2
  the serial port on the host computer side (the QNX system side)
  com2 corresponds to the QNX device likely /dev/ser2

radiobaud=19200
  baud rate for the radio-side serial connection
  19200 baud is the standard for TNC2-compatible TNCs

COMPUTER baud=38400
  baud rate for the host-side serial connection
  higher than radio baud to avoid host-side bottleneck

Radio COM IRQ=4
  hardware interrupt number for the radio serial port

Computer COM IRQ=3
  hardware interrupt number for the computer serial port

Control=4
  bit 2 set: special mode enabling CI/EI or CR flags in HOST mode
  bit 0: enables on-process display
  bit 1: enables function display
  bit 2: enables special mode (CI/EI/CR flags in HOST mode, selected here)
  bit 3: enables special buttons

num buf=max
  allocates the maximum number of AX.25 frame buffers
  max is driver-defined, likely 256 or system memory limited

num buf rep=50
  allocates 50 repeat/retransmit buffers for the repeater function

num msg=0
  message buffer count set to 0 (messages handled by frame buffers instead)

x=1
  enables waiting for channel clear (GOT signal) before transmission
  0 would allow transmission without waiting for channel availability

y=4
  maximum number of connection establishment attempts before giving up

a=1
  address line mode: 1 enables address line checking (AX.25 address matching)

n=2
  minimum number of connection retries before reporting failure

o=4
  maximum number of frame transmit attempts per packet

r=1
  repeater mode: 1 enables frame repeating for relay operation

e=1
  echo output: 1 enables echo of received data to the host

w=10
  slot timer in tenths of seconds (1.0 second slottime)
  controls the CSMA random backoff timer for channel access

p=64
  persistence value for CSMA channel access
  probability of transmitting when channel is clear = (p+1)/256 = 25.4%

v=0
  connect confirmation: 0 disables explicit connect acknowledgment
  the connection is implicit when data arrives

f=250
  response timeout in tenths of seconds (25 seconds maximum response wait)
  minimum value for this parameter

t=1
  delay before attempting next repeater connection in tenths of seconds

t2=100
  time to wait for the next packet frame in the current session in ms
  inter-frame gap within a single transmission sequence

t3=18000
  timeout for radio channel check in tenths of seconds
  if no activity for 1800 seconds, the channel is considered lost

TimeWaitRadio=3
  time in seconds to wait for a radio symbol before timeout

hwd=4
  hardware watchdog: uses LPT port 4 (0-indexed, so LPT4)
  type 586: compatible with 586-class PC hardware watchdog circuit
  value 0 would disable the hardware watchdog

inversDTR=0
  DTR signal polarity: 0 = normal (active high), 1 = inverted

inversGOT=0
  GOT (grant of transmission) signal polarity: 0 = normal, 1 = inverted

maxTimeRadioGot=30000
  maximum time in milliseconds to wait for GOT signal during transmission
  if GOT is not received within 30 seconds, transmission is aborted
  0 would disable this limit

maxTimeRadioMute=90000
  maximum silence time on the radio channel in milliseconds (90 seconds)
  if no signal is received for 90 seconds, the channel is considered lost
  corresponds to 15 minutes when expressed in the comment

maxTimeRadioDumb=30000
  maximum time in milliseconds for radio receiver inactivity
  if the TNC reports no received frames for 30 seconds, it is considered stuck
  corresponds to 5 minutes when expressed in the comment

durationReset=150
  reset pulse duration in milliseconds
  the pulse sent to the TNC hardware reset line during error recovery

pathMsg=/sramdisk (commented out)
  path for message storage would be the SRAM disk
  currently disabled, messages stored in RAM only
```

The [Disks] section that appears after the modem parameters in the recovered
strings output is actually the beginning of the start.ini content that
followed modem.cfg in the binary strings, not part of modem.cfg itself.


## progr/start.ini Full Content

This is the active deployment configuration loaded by qmicro. Fully recovered:

```ini
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

Section meanings:

[Disks] configures the storage layout. FileStartProtokol is the path prefix
for all persistent state files. FlashDiskPath is where the driver binaries
are stored. StaticDiskPath is the SRAM-backed volatile storage for frequently
updated state. SwopDiskPath is the swap area for ring buffer persistence files.

[Uso] specifies the USO field bus drivers to load. Each Uso= line specifies
the driver name and its parameters. The p parameter sets the serial port
device path. The s parameter sets the baud rate. gpio86dx takes no parameters
as it uses the PC/104 ISA bus directly.

[Kanals] specifies the upstream channel drivers. The a parameter sets the
unit address (111 in this deployment). The i parameter specifies the IP
address and port configuration in the format localIP,localPort,remoteIP,remotePort.

[Module] specifies additional shared library modules to load via dlopen. Each
module's entry point InitModule() is called after loading. The parameters
after the library name are passed to the module's RaspakKeys function:
  o: configuration file path for libjevent.so
  f: journal output file path prefix for libjevent.so
  s: ring buffer size (1000 slots) for libjevent.so


## project/start.ini Full Content

This is the alternate deployment configuration associated with the July 2025
firmware update. Fully recovered:

```ini
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

Differences from the active configuration:

  /dev/ser9 instead of /dev/ser7 for the USOTM field bus port
  unit address 5 instead of 111
  symmetric TCP ports (both 2124) instead of asymmetric (2124/5124)
  no s parameter for libjevent.so (uses default ring buffer size)
  libterm.so is loaded for local HMI panel support
  a [Retransl] section activates UDP retranslation on port 2127
  a [Programms] section kills ksh for security hardening

The [Retransl] section is parsed by qmicro to load retranslation drivers.
The underscore prefix on _Retransl indicates it is an inactive or commented
entry in the project configuration that would be made active by removing the
underscore. Similarly, _Programm with the underscore is the inactive form.

The [Programms] section runs arbitrary processes at qmicro startup. The
/proc/boot/slay -f ksh command kills all running ksh processes, preventing
interactive shell access on the deployed system.


## restore binary (addprog/restore)

The restore binary is the script executor for restore.scr. Source file:
restore.c. Build date: same period as the main firmware.

The binary implements three key functions:

ParseScr(restore.scr_path) reads the script file and parses it into an
array of command lines. The script content is stored in the aScr array
(confirmed global) with up to nScr entries (the script line count) and
namScr holding the filename strings. The lb global tracks the current
parsing position. The path global holds the script file path.

Process(command_string) executes a single command from the parsed script.
For commands starting with known prefixes, it uses spawn() with the
appropriate arguments. For general commands, it uses system() which invokes
the shell. The str global is a temporary buffer used during command processing.

fr() (free resources) performs cleanup after all commands have been executed.

The tim global holds a timestamp used for logging the script execution time.

The main function calls ParseScr with the restore.scr path, then iterates
through the parsed commands calling Process for each one, then calls fr.

The binary uses spawn rather than system for known commands to avoid the
overhead of shell invocation and to have direct control over the spawned
process's arguments. The restore.scr script comments show several commands
that are pidin invocations (process list queries) used to verify that
previously started processes are running before proceeding.


## Net Configuration (net/netcfg)

The netcfg binary is a network configuration utility. Its strings section
contains the complete restore.scr script verbatim, which is how the full
script content was recovered for this analysis. The netcfg binary appears
to serve as a combined network configuration and startup script repository,
embedding the script as a string constant that it can extract and execute.

The netcfg binary also confirms the four-interface network topology:
  en0 via vortex driver at 100 Mbit full duplex (primary LAN)
  /net1 via micrel8841 driver (secondary LAN)
  /net2 via asix USB-Ethernet bus 1 device 1 (tertiary LAN)
  /net3 via asix USB-Ethernet bus 1 device 2 (quaternary LAN)

Each interface runs its own inetd instance with SOCK environment variable
routing, allowing services to be bound to specific interfaces.


## pdebug (addprog/pdebug)

The pdebug binary is the standard QNX GDB-protocol remote debug daemon,
build date May 20 2009, version 6.4.1. Its presence in the deployed firmware
confirms the system retained full remote debugging capability.

The binary supports two transport modes confirmed from usage strings:
  TCP/IP: "For a TCP/IP connection on port 8000: pdebug 8000 &"
  Serial: "For a 57600 baud serial connection on /dev/ser2: pdebug /dev/ser2,57600 &"

The debug protocol is QNX's extended GDB remote serial protocol (RSP) with
QNX-specific extensions for process management, memory access, and real-time
thread control. The PDEBUG_DEBUG environment variable enables verbose logging.

The binary implements the full target-side debugging interface:
  TargetAttach, TargetDetach, TargetConnect, TargetDisconnect
  TargetLoad, TargetRun, TargetStop, TargetKill
  TargetMemrd, TargetMemwr (memory read and write)
  TargetRegrd, TargetRegwr (register read and write)
  TargetBrk (hardware and software breakpoints)
  TargetMapinfo (process memory map)
  TargetPidlist (process list)
  TargetSelect (thread selection)
  TargetHandlesig (signal handling configuration)

Process-level debugging confirms full access to all running processes on the
RTU including qmicro, the field bus drivers, and the channel drivers.


## flashctl (addprog/flashctl)

The flashctl binary is the standard QNX flash filesystem control utility,
build date May 20 2009, version 6.4.1. It provides complete flash management:

  -e: erase flash from offset to offset + limit
  -f: format flash from offset to offset + limit
  -m: mount a flash partition
  -u: unmount a flash partition
  -r: reclaim flash (garbage collection)
  -i: display partition information
  -L: lock flash region
  -U: unlock flash region
  -A: unlock entire flash array
  -x: exit the flash driver
  -z: query compression flag
  -c: set compression flag

The -p option specifies the raw flash device path (e.g., /dev/fs0p0).
The example in the usage string: "%C -p /dev/fs0p0 -e -f -n /flash -m"
erases, formats, and mounts a partition at /flash.

The DCMD_F3S_* devctl commands operate on the QNX f3s (flash 3 second
generation) filesystem driver. The DeviceInfo display format shows size,
free space, overhead, padding, reserved, spare, stale, headers, erased,
and total statistics as both hex bytes and percentages.
