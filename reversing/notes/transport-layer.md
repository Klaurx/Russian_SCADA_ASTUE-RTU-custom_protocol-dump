# Transport Layer Driver Analysis

## Overview

The transport layer in this firmware consists of all drivers that subclass
kanaldrv and implement the RTOS link layer over various physical transports.
The kanaldrv base class provides the Obmen() session loop, the srez frame
construction, and the command dispatch table. Each transport subclass provides
the physical read and write operations by implementing a specific set of
virtual method slots.

All transport drivers share the same binary structure: a main() function
that instantiates the transport class, calls RaspakKeys() to parse command
line arguments, and then calls StartDrv() which spawns the Obmen() working
thread. The transport object persists for the lifetime of the process.

The transport drivers are loaded by qmicro from the [Kanals] section of
start.ini. Each Kanal= line specifies the driver binary name and its
parameters. The a parameter sets the unit address (stored at obj+0x4 in the
kanaldrv object), and the i parameter sets IP and port configuration for
network transports.


## ctcpqnx TCP Channel (progr/tcpqnx)

Class name: ctcpqnx. Source file: qnxgprs.cc (not tcpqnx.cc).

This is a critical finding: the strings section of progr/tcpqnx contains
the source file name qnxgprs.cc rather than tcpqnx.cc or ctcpqnx.cc. The
global variable gsmlink at 0x0804ac40 is also present. This confirms that
the progr/tcpqnx binary is the GPRS-specific TCP channel driver compiled
from the qnxgprs.cc source, not a general-purpose TCP driver.

The binary links against libkanaldrv.so.1 and libsocket.so.2. It requires
the initkandrv vtable functions for the initialisation handshake sequence:
AnalizBufInitPriem, InitBuffers, _InitKanal, initkandrv constructor and
destructor, InitSend, PriemKvitEvent, SendInitBuffer.

Methods confirmed from nm:

```
ctcpqnx::ctcpqnx()                    default constructor
ctcpqnx::~ctcpqnx()                   destructor, three variants
ctcpqnx::CloseInitKanal()             close the init-phase TCP channel
ctcpqnx::CloseSeans()                 close the data-phase TCP session
ctcpqnx::ErrorInitKanal()             handle initialisation failure
ctcpqnx::GetStaticTimeSend()          returns static transmit timing (0.0)
ctcpqnx::InitKanal()                  TCP connect and bind sequence
ctcpqnx::RaspakKeys(int, char**)      parse command line: a, i parameters
ctcpqnx::ReadByteFromInitKanal(unsigned char*)  read from init channel
ctcpqnx::ReadByteFromKanal(unsigned char*)      read from data channel
ctcpqnx::ReadByteKvitokFromInitKanal(int, unsigned char*)
  read a kvitok byte during the initialisation handshake
ctcpqnx::ReadInitStartKvitok(int)
  read and validate the initial handshake kvitok from the SCADA master
ctcpqnx::SendBufToKanal(unsigned char*, unsigned short)  transmit data
ctcpqnx::SendInitBufToKanal(unsigned char*, unsigned short)  transmit init
ctcpqnx::SendInitKvitokBuffer(int)    send the init-phase kvitok
ctcpqnx::TimeReInitKanal()            close and reopen the TCP connection
```

The ctcp global at 0x0804bea0 is the singleton ctcpqnx instance.

The ReadKvitok method (imported as kanaldrv::ReadKvitok(unsigned short))
is used during the initialisation handshake to read the SCADA master's
sequence acknowledgment. This import is present in tcpqnx but absent from
the simpler udpqnx, confirming the TCP transport has a more complex
initialisation handshake than UDP.

The MakeAdrBuf and SendBuffer methods are imported from kanaldrv and confirm
this transport participates in the full RTOS address frame and buffer
transmission protocol.

The local IP and port are stored at object offsets +0x20034 (local IP string)
and +0x2005c (local port), and the remote IP and port at +0x20048 (remote IP)
and +0x2005e (remote port), parsed from the i command line parameter.


## ctcpqnx TCP Channel Variant 2 (progr/tcpqnx second binary)

A second ctcpqnx binary exists in the firmware with a different method set.
This variant (confirmed from the simpler nm output) omits the init-phase
methods (ReadByteFromInitKanal, ReadByteKvitokFromInitKanal, ReadInitStartKvitok,
SendInitBufToKanal, SendInitKvitokBuffer, ErrorInitKanal, CloseInitKanal)
and adds CloseInitKanal to the deletion sequence. This simpler variant is
the standard TCP data channel without the full RTOS handshake protocol. It
imports from initkandrv for the base class initialization but delegates the
init sequence entirely to initkandrv rather than implementing it.


## ctcpqnx Alarm Channel (progr/tcparm)

Class name: ctcpqnx (same class). Source file: tcparm.cc.

The critical difference from the other ctcpqnx variants: tcparm links against
libkanalarm.so.1 rather than libkanaldrv.so.1. The libkanalarm.so.1 library
has not been recovered as a standalone file in the firmware image, suggesting
it is either statically linked into tcparm or resides in a firmware partition
not included in this analysis set.

The method set is the same as the simpler ctcpqnx variant, with the same
socket functions but without the extended init-phase methods. The alarm
channel semantics likely involve different timeout values, priority queuing
for alarm events, or a distinct acknowledgment protocol that prevents alarm
messages from being held behind regular data traffic.

tcparm is loaded when the [Kanals] section of start.ini includes a Kanal=tcparm
line. In the recovered configurations (both progr/start.ini and project/start.ini),
tcparm is not active, but its presence in the binary set confirms it is available
for deployments that require a separate alarm channel.


## cudpqnx UDP Channel (progr/udpqnx)

Class name: cudpqnx. Source file: identified from class typeinfo.

The binary links against libkanaldrv.so.1 and libsocket.so.2. It uses
recvfrom and sendto rather than recv and send, confirming datagram-mode UDP
without connection establishment. select is used for receive readiness checking
with timeout.

The My_Port global at 0x0804a9e0 holds the local bind port. The MyIpAdr
global at 0x0804ab40 holds the local IP address string for binding. These
are parsed from the command line by RaspakKeys.

Methods confirmed from nm:

```
cudpqnx::cudpqnx()                 constructor, two variants
cudpqnx::~cudpqnx()                destructor, three variants
cudpqnx::InitKanal()               bind UDP socket and configure address
cudpqnx::RaspakKeys(int, char**)   parse command line parameters
cudpqnx::ReadByteFromKanal(unsigned char*)  recvfrom one byte with timeout
cudpqnx::SendBufToKanal(unsigned char*, unsigned short)  sendto
```

The cudp global at 0x0804ab60 is the singleton cudpqnx instance.

The udpqnx binary does not require initkandrv, confirming UDP has no session
initialisation handshake. The transport is connectionless: each frame is sent
as an independent datagram and the RTOS sequence numbers in the SOST_SEND
struct provide the only session continuity.

The GetDinamicTimeSend and GetLenPachka methods are imported from kanaldrv,
confirming that the UDP transport uses dynamic timing (adjusting its transmit
interval based on channel load) and supports the pachka (packet batch)
framing used by the retranslation layer.


## cudpretr UDP Retranslation Channel (progr/udpretransl)

Class name: cudpretr. Source file: udpretransl.cc.

The binary links against libretransldrv.so.1 (not libkanaldrv.so.1),
confirming this transport operates in the retranslation layer rather than
the primary channel layer. It uses the retransldrv base class vtable rather
than the kanaldrv vtable.

The My_Port global at 0x0804a890 and MyIpAdr global at 0x0804a9c0 provide
the local binding parameters. The retranslation transport listens for
retranslated srez frames from downstream retranslators and forwards them
upstream to the SCADA master.

Methods confirmed from nm:

```
cudpretr::cudpretr()               constructor, two variants
cudpretr::~cudpretr()              destructor, three variants
cudpretr::CloseSocket()            close the UDP socket
cudpretr::InitKanal()              bind and configure the UDP socket
cudpretr::OpenSocket()             create and bind the retranslation socket
cudpretr::RaspakKeys(int, char**)  parse command line parameters
cudpretr::ReadByteFromKanal(unsigned char*)   recvfrom with timeout
cudpretr::SendBufToKanal(unsigned char*, unsigned short)  sendto
```

The cudp global at 0x0804a9e0 is the singleton cudpretr instance.

The project/start.ini configuration activates udpretransl on port 2127:
_Retransl=udpretransl p2127


## csercom Serial Channel (progr/sercom)

Class name: csercom. Source file: sercom.cc.

The binary links against libkanaldrv.so.1. It uses direct serial port file
descriptor operations: open, read, write with termios configuration.

The sercom global at 0x0804a6a0 is the singleton csercom instance.

Methods confirmed from nm:

```
csercom::csercom()                 constructor, two variants
csercom::~csercom()                destructor, three variants
csercom::ClearKanal()              clear the serial port state
csercom::GetDinamicTimeSend(unsigned short)   dynamic transmit timing
csercom::GetStaticTimeSend()       static transmit timing (returns 0.0)
csercom::InitKanal()               open and configure the serial port
csercom::RaspakKeys(int, char**)   parse command line parameters
csercom::ReadByteFromKanal(unsigned char*)    read one byte with timeout
csercom::SendBufToKanal(unsigned char*, unsigned short)  write frame
csercom::TimeReInitKanal()         close and reopen the serial port
```

csercom does not require initkandrv or libsocket.so.2, confirming it is a
pure serial transport with no TCP/IP component.

The GetDinamicTimeSend method is present in csercom but returns a computed
value based on the baud rate and current channel load, allowing the RTOS
layer to adapt its transmit scheduling to the physical serial link speed.

csercom is the parent class of cgsmlink. The GSM modem transport uses all
of csercom's serial handling and adds the AT command layer on top.


## cgsmlink GSM Modem Channel (progr/gsmlink)

Class name: cgsmlink. Source file: gsmlink.cc.

The binary links against libkanaldrv.so.1. It contains both cgsmlink methods
and csercom methods, confirming csercom is statically inherited or the binary
includes both class implementations.

The gsmlink global at 0x0804ac40 is the singleton cgsmlink instance.

The AT command string "AT+CFUN=1" confirmed from strings, used by modem_on()
to power on the GSM modem radio.

Methods confirmed from nm:

```
cgsmlink::cgsmlink()               constructor, two variants
cgsmlink::~cgsmlink()              destructor, three variants
cgsmlink::InitKanal()              configure modem and open data connection
cgsmlink::modem_off()              send power-down AT commands to modem
cgsmlink::modem_on()               send AT+CFUN=1 and establish GPRS link
cgsmlink::RaspakKeys(int, char**)  parse command line parameters
cgsmlink::TimeReInitKanal()        disconnect and reconnect GPRS session
csercom::ClearKanal()              clear serial port state
csercom::GetDinamicTimeSend(unsigned short)
csercom::GetStaticTimeSend()
csercom::InitKanal()               serial port open and configure
csercom::RaspakKeys(int, char**)
csercom::ReadByteFromKanal(unsigned char*)
csercom::SendBufToKanal(unsigned char*, unsigned short)
csercom::TimeReInitKanal()
```

The two-level class structure allows TimeReInitKanal in cgsmlink to first
disconnect the GPRS data session (modem-level), then call csercom::TimeReInitKanal
to reset the serial port (hardware level). This two-phase reconnect handles
both modem state and serial state in the correct order.


## ctnc TNC Packet Radio Channel (progr/tnc)

Class name: ctnc. Source file: tnc.cc.

The binary links against libkanaldrv.so.1. It requires devctl for hardware
control of the serial port beyond what termios provides (likely for RTS/CTS
and DSR/DTR hardware handshake control used by the TNC).

The tnc global at 0x0804c1c0 is the singleton ctnc instance. The S global
at 0x0804be34 holds the TNC configuration string.

Methods confirmed from nm:

```
ctnc::ctnc()                constructor, two variants
ctnc::~ctnc()               destructor, three variants
ctnc::AnalizAnswer(unsigned char*, int)
  analyzes a received TNC command response
  the int parameter is the expected response type code
ctnc::CheckCTS(int)
  reads the CTS (clear to send) hardware signal
  the int parameter is the timeout in milliseconds
ctnc::CheckDSR(int)
  reads the DSR (data set ready) hardware signal
  used to verify the TNC is powered and connected
ctnc::ClearKanal()
  resets the AX.25 channel state to idle
ctnc::GetDinamicTimeSend(unsigned short)
  returns dynamic transmit timing based on channel occupancy
ctnc::GetLenPachka(unsigned short)
  returns the maximum frame length for the current channel state
ctnc::GetStaticTimeSend()
  returns fixed transmit timing (returns 0.0 via fldz)
ctnc::InitRadioKanal()
  sends TNC2 init sequence: JHOST1, P255, QRES
  JHOST1 enables host mode on the TNC2 (binary packet mode)
  P255 sets maximum packet length to 255 bytes
  QRES resets the TNC2 station state
ctnc::InitTnc()
  full TNC initialization: configures callsign, timers, and operating mode
ctnc::OprosAllChannels()
  polls all active AX.25 virtual circuits for received data
ctnc::RaspakKeys(int, char**)
  parses command line: computer port, radio port, baud rates, callsign
ctnc::RaspakSaveSost(char*)
  parses saved channel state from the state file
ctnc::ReadByteFromKanal(unsigned char*)
  reads one byte from the received AX.25 frame buffer
ctnc::ReadDataFromChannel(unsigned char)
  reads a complete data frame from the specified AX.25 channel
ctnc::ReadInfoPachka(unsigned char)
  reads the information field of an AX.25 I-frame
ctnc::ReadMessage(unsigned char)
  reads and dispatches a TNC message by type
ctnc::ReadSaveSost()
  reads the saved channel state from the state file at startup
ctnc::ReadSaveSostKanal(unsigned char)
  reads the saved state for a specific AX.25 channel index
ctnc::ReadSostKanal(unsigned char)
  reads the current operating state of an AX.25 channel
ctnc::SendBufToKanal(unsigned char*, unsigned short)
  transmits a frame via the AX.25 radio channel
ctnc::SendCommandHost(unsigned char, unsigned char, unsigned char*,
                      unsigned long, unsigned char*, unsigned long,
                      unsigned long*, unsigned long)
  sends a TNC2 host-mode command frame to the TNC
  parameters: channel, command_type, data1, len1, data2, len2,
              result_ptr, timeout
ctnc::SendCommandTerminal(char*, char*, unsigned long, unsigned long*,
                           unsigned long)
  sends an AT-style ASCII command to the TNC
  parameters: command_string, response_string, response_maxlen,
              received_len_ptr, timeout
```

The TNC2 command strings confirmed from the strings section:

  JHOST1 enables host mode (binary framing instead of ASCII terminal mode)
  P255 sets the maximum packet length parameter to 255 bytes
  QRES sends a quit/reset command to clear pending sessions
  "%ld %ld %ld %ld %ld %ld %ld" is the 7-field status format for logging
    channel statistics: send count, receive count, error count, retry count,
    connect count, disconnect count, and currently connected flag
  "I%ld" is the information frame count format
  "S%ld" is the supervisory frame count format
  "@T2%ld" is a timer 2 status format (T2 is the AX.25 inter-frame gap timer)

The two serial ports used by ctnc (computer port and radio port) have
independent baud rates, interrupt assignments, and timing parameters.
The computer port connects to the host (this QNX system) and the radio port
connects to the physical TNC hardware.

The modem.cfg file (documented in docs/architecture.md) configures the radio
port as /dev/ser4 at 19200 baud and the computer port as com2 (likely
/dev/ser2) at 38400 baud.


## crs485retr RS-485 Retranslation Channel

### Driver mode variant (progr/rs485retransl)

Class name: crs485retr. Source file: rs485retransl.cc.
Links against libretransldrv.so.1.

### DOS compatibility variant (progr/rs485dosretransl)

Class name: crs485retr. Source file: rs485dosretransl.cc.
Links against libretransldos.so.1.

Both variants implement the same crs485retr class and differ only in the
retranslation library they link against. The NIAM build marker suffix for
both is @ (0x40), confirming they share the same unit-type discriminator.

Methods confirmed from nm (identical in both variants):

```
crs485retr::crs485retr()           constructor, two variants
crs485retr::~crs485retr()          destructor, three variants
crs485retr::InitKanal()            open and configure the RS-485 port
crs485retr::RaspakKeys(int, char**)  parse command line parameters
crs485retr::ReadByteFromKanal(unsigned char*)   read one byte
crs485retr::SendBufToKanal(unsigned char*, unsigned short)  write frame
```

The crs485 global holds the singleton instance. The default port is /dev/ser1.
The RS-485 retranslation path aggregates srez data from downstream RTU
devices and forwards it upstream via the retransldrv base class.

The PrepareBufRetr method is imported from retransldrv (not kanaldrv),
confirming the retranslation layer has a distinct buffer preparation path
from the primary channel layer.

libretransldos.so.1 provides DOS-mode compatibility for older RTU devices
that expect a DOS-style communication handshake. libretransldrv.so.1 provides
the driver-mode implementation for QNX-native communication.


## cp104send IEC 60870-5-104 Server (progr/p104send)

Class name: cp104send. Source file: p104send.cc.

The cp104send class subclasses kanaldrv and implements a complete IEC
60870-5-104 TCP server. Full documentation is in docs/protocol.md.

The binary links against libkanaldrv.so.1 and libsocket.so.2. It does not
link against libusodrv.so.1, confirming it operates on the channel side of
the firmware rather than the field bus side.

The TimeCorrectGo(SOST_TIME_CORRECT*, unsigned short, long) function is a
standalone exported function at 0x08049e59, not a class method. It applies
GPS-derived time correction to the IEC 104 time base, converting between the
RTOS internal SOST_TIME_CORRECT representation and the CP56Time2a 56-bit
timestamp format required by IEC 104.

The WorkProc function at 0x080497a6 is the thread entry point for the IEC 104
server. It is a free function (not a class method) spawned by StartDrv().

The ctcp global at 0x0804fa00 is the singleton cp104send instance.
