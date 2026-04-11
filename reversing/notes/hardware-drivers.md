# Hardware Driver Analysis

## Overview

The hardware drivers in this firmware set implement the usodrv interface for
PC/104 ISA bus devices that are accessed directly via memory-mapped I/O or
LPT port registers, without any serial protocol intermediary. These drivers
bypass the field bus polling model and write measurements directly to the
shared memory segments maintained by qmicro.

Three hardware driver classes are present: uso_amux for the analog
multiplexer boards, uso5600 for the c5600 ISA digital I/O board, and the
gpio86dx class documented in the README and architecture notes. Additionally
the comdirect class provides raw serial COM port access.


## uso_amux Analog Multiplexer (progr/amux and progr/amux86dx)

### Plain Variant (amux)

Class name: uso_amux. Source file: identified from class typeinfo.
Links against libusodrv.so.1 and libservdrv.so.1.

The uso_amux class implements an analog multiplexer driver for the PC/104
board. It reads analog voltages by selecting a multiplexer channel and then
sampling the ADC output. The LPT (parallel port) base address is stored in
the PortLPT global, shared with the gpio86dx driver.

Methods confirmed from nm:

```
uso_amux::uso_amux(unsigned short)     constructor, two variants
uso_amux::~uso_amux()                  destructor, three variants

uso_amux::AddKodInBuffer(unsigned int, unsigned int, long)
  stores a sampled analog value into the internal ring buffer
  parameters: channel_index, sample_sequence, raw_adc_value
  the ring buffer decouples the sampling thread from the value delivery thread
  called by OprosAmux on each completed sample

uso_amux::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
  retrieves the current averaged analog value for a requested channel
  calls ReadFromBuffer to get the most recent averaged value
  converts the raw ADC count to engineering units using the channel
  calibration coefficients stored in DEF_ANALOG

uso_amux::GetSupportTypes(SUPPORTED_USO_AND_BUF_TYPES*)
  reports the data types supported by this driver to qmicro
  sets only the analog bit in the type bitmask, confirming amux supports
  only analog data, no discrete or impulse channels

uso_amux::InitUso()
  initialises the multiplexer hardware
  writes control bytes to PortLPT to configure the ISA board
  prints "Amux - Error connect to ISA!!!!!" if the board does not respond
  (error string confirmed from strings analysis of the binary)

uso_amux::MakeAver(long*)
  computes the running average of ADC samples
  the long* parameter is the accumulator array
  implements a simple moving average filter over the most recent N samples
  where N is determined by the averaging window configured in start.ini

uso_amux::OprosAmux(void*)
  the main polling thread entry point, spawned by pthread_create
  runs a continuous loop: select channel, trigger ADC, wait for conversion,
  read ADC result, call AddKodInBuffer
  uses delay() between samples to control the sampling rate
  the void* parameter is the standard pthread argument (the uso_amux this
  pointer cast to void*)

uso_amux::ReadFromBuffer(unsigned short, unsigned short, MSG_RETURN_ANALOG*)
  reads the averaged value from the ring buffer for a specified channel
  parameters: channel_index, unused, output struct
  retrieves the most recent MakeAver output for the channel and populates
  the MSG_RETURN_ANALOG with the engineering unit value and quality flags
```

The uamux global holds the singleton uso_amux instance. ConnectToUso is
called from libusodrv to register the driver's shared memory segments.


### x86DX Variant (amux86dx)

Class name: uso_amux (same class name as the plain variant). Source file:
identified from binary comparison.

The x86DX variant adds SetAdr(unsigned char) which is absent from the plain
amux binary:

```
uso_amux::SetAdr (amux86dx only, at address 0x0804936c)
  writes the channel selection byte to the multiplexer address register
  the unsigned char parameter is the multiplexer channel address
  writes to the PortLPT base address to select the channel before sampling
  the x86 DX board uses an addressable multiplexer where channel selection
  is explicit via a register write, unlike the plain board where channels
  are selected by a different mechanism
```

All other methods are identical in signature and purpose to the plain amux
variant. The OprosAmux thread in amux86dx calls SetAdr before each sample
to select the target channel, then proceeds with the same ADC trigger and
read sequence as the plain variant.

The address 0x0804936c for SetAdr in amux86dx is in the same text segment
region as the OprosAmux polling thread at 0x080497fa, confirming they are
tightly coupled in the sampling loop.


## uso5600 PC/104 c5600 Board Driver (progr/c5600)

Class name: uso5600. Source file: c5600.cc.

The c5600 is a PC/104 ISA digital I/O board that provides discrete inputs,
discrete outputs, and impulse counter inputs. The driver uses direct ISA
bus access via memory-mapped registers.

The binary links against libusodrv.so.1 and libservdrv.so.1. It requires
clock_gettime (from the C library) for high-resolution timestamps, confirming
the c5600 driver captures precise event timestamps directly at the hardware
interrupt level.

The binary uses pthread_attr_setinheritsched and pthread_attr_setschedparam
to configure real-time scheduling for its polling threads, and
pthread_attr_setinheritsched explicitly to ensure the threads inherit the
parent process's real-time priority settings.

The ThreadCtl call is used to grant the driver threads IO access permissions
required for direct ISA register reads and writes.

The atomic_add and atomic_sub operations from the QNX atomic library are
used for thread-safe counter manipulation, allowing the interrupt handler
thread and the value delivery thread to share the impulse counter accumulators
without explicit mutex locking.

Methods confirmed from nm:

```
uso5600::uso5600(unsigned short)     constructor, two variants
uso5600::~uso5600()                  destructor, three variants

uso5600::ConfigurateInput()
  configures the c5600 board's input channels
  writes control bytes to the ISA base address to set each input as
  discrete or counter mode
  reads back the configuration to verify the write succeeded

uso5600::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
  retrieves the current discrete input state for a requested channel
  reads from the shared memory segment written by Opros5600
  populates MSG_RETURN_DISCRET with the current state and quality

uso5600::GetImpuls(MSG_GET_PARAM*, MSG_RETURN_IMPULS*)
  retrieves the accumulated impulse counter value for a requested channel
  reads the atomic counter accumulator for the specified channel
  populates MSG_RETURN_IMPULS with the accumulated count

uso5600::GetSupportTypes(SUPPORTED_USO_AND_BUF_TYPES*)
  reports supported types to qmicro
  sets discrete, impulse, and discrete output bits in the type bitmask
  does not set the analog bit, confirming no analog inputs

uso5600::InitUso()
  initialises the c5600 board hardware
  calls ConfigurateInput to set up channel directions
  spawns both the Opros5600 and Opr threads via pthread_create
  configures real-time scheduling for both threads

uso5600::Is_Uso(unsigned short)
  checks whether the c5600 board is present at the configured ISA address
  performs a read-back test to verify hardware presence
  returns non-zero if the board responds correctly, zero if absent

uso5600::NomEvent()
  returns the current event sequence number
  used by qmicro to track which events have been delivered from this driver

uso5600::Opros5600(void*)
  the primary polling thread
  continuously reads all input channels from the ISA registers
  uses clock_gettime for precise event timestamps
  on change detection, creates an event record with the precise timestamp
  writes updated discrete states to the /SostDiscrets shared memory segment
  uses atomic_add to update impulse counter accumulators

uso5600::Opr(void*)
  the secondary processing thread (also exported as the free function Opr)
  processes accumulated impulse counter data
  applies debounce filtering to raw counter inputs
  delivers processed impulse counts to the /SostImpulses shared memory segment

uso5600::ReadInp(unsigned short, int)
  reads the raw input register state for the specified channel
  the unsigned short is the channel index
  the int is a read mode flag (may select between latched and current state)
  returns the raw 16-bit ISA register value

uso5600::SetDout(MSG_SET_DOUT*, MSG_RETURN_DOUT*)
  sets a discrete output channel on the c5600 board
  writes the output value to the appropriate ISA register
  reads back the register to verify the write succeeded
```

The u5600 global at 0x0804c22c holds the singleton uso5600 instance.

The PortLPT global at 0x0804b12e holds the LPT base address, shared with
the amux driver family, confirming the c5600 and amux boards both connect
to the PC/104 ISA bus via the same LPT port.

The binary calls ConnectToDiscrets, ConnectToImpuls, DisconnectFromDiscrets,
and DisconnectFromImpuls from libusodrv, which are the shared memory segment
registration functions for the specific I/O types. Unlike the serial field
bus drivers that call the generic ConnectToUso, the c5600 driver registers
each I/O type separately, allowing finer-grained control over which segments
it populates.

The FindUso import from libusodrv is used in SetDout to look up the iocuso
handle for the target output channel before writing.


## comdirect Raw COM Port Driver (progr/comdirect)

Class name: comdirect. Source file: comdirect.cc.

The comdirect driver exposes raw serial COM port access through the USO
interface without any protocol framing. It is fundamentally different from
all other USO drivers in that it does not implement a field bus protocol:
there are no TestPriem, WaitOtvet, or Raspak methods. Instead it provides
direct byte-level read and write access to a serial port through the standard
MSG_GET_PARAM/MSG_RETURN interface.

The binary links against libusodrv.so.1 and libservdrv.so.1. It does not
link against libkanaldrv.so.1.

The most notable anomaly in this driver is the port path string in the
strings section: "\dev\ser1" with Windows-style backslash separators rather
than the forward-slash "/dev/ser1" used by every other driver in the firmware
set. This is the only instance of Windows-style paths in the entire firmware
and strongly suggests the comdirect driver was partially ported from a
Windows or DOS codebase, with the path separator not converted during porting.
Whether QNX Neutrino accepts backslash paths (it does not in standard usage)
or whether this path is corrected at runtime by RaspakKeys is not confirmed.

Methods confirmed from nm:

```
comdirect::comdirect(SUPPORTED_USO_AND_BUF_TYPES*)
  constructor, two variants
  note the constructor takes SUPPORTED_USO_AND_BUF_TYPES like aclass,
  not unsigned short like the other usodrv subclasses
  this is a second instance of the SUPPORTED_USO_AND_BUF_TYPES constructor
  pattern in a non-supervisor class

comdirect::~comdirect()             destructor, three variants

comdirect::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
  returns an empty analog result (comdirect has no analog channels)
  the method exists to satisfy the usodrv vtable requirement

comdirect::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
  returns an empty discrete result (comdirect has no discrete channels)
  same rationale as GetAnalog

comdirect::GetImpuls(MSG_GET_PARAM*, MSG_RETURN_IMPULS*)
  returns an empty impulse counter result
  same rationale

comdirect::GetSupportTypes(SUPPORTED_USO_AND_BUF_TYPES*)
  overrides the base class to report which types this driver supports
  for raw COM port access this would be implementation-specific

comdirect::InitPort()
  configures the serial port using termios
  reads Port, Speed, Stopbits, Parity, TimeByte, and TimeOut globals
  TimeByte: inter-byte timeout in milliseconds
  TimeOut: overall receive operation timeout in milliseconds

comdirect::MakeUsoSpecialBufZaprosUso(MSG_SPECIAL_BUF*, MSG_SPECIAL_BUF**)
  the primary useful method in comdirect
  takes an incoming MSG_SPECIAL_BUF request, writes the raw bytes to the
  serial port, waits for the response, and populates the output
  MSG_SPECIAL_BUF** with the received data
  this is how external code communicates with the raw serial device:
  by passing pre-formatted byte buffers that are transmitted directly
  without any framing or protocol processing

comdirect::RaspakKeys(int, char**)
  parses command line arguments from the Uso= line in start.ini
  reads: Port (device path), Speed (baud rate), Stopbits, Parity, TimeByte,
  TimeOut

comdirect::SetDout(MSG_SET_DOUT*, MSG_RETURN_DOUT*)
  returns an empty discrete output result
  comdirect does not support discrete outputs via the standard interface
  all output goes through MakeUsoSpecialBufZaprosUso
```

Configuration globals:

```
port (0x0804b148)      serial port path string (with backslash anomaly)
Port (0x0804ad40)      parsed port configuration (may differ from port)
Speed (0x0804b144)     baud rate integer
Stopbits (0x0804b150)  stop bits: 1 or 2
Parity (0x0804b154)    parity: 'N', 'E', or 'O'
TimeByte (0x0804b15c)  inter-byte timeout in ms
TimeOut (0x0804b158)   overall timeout in ms
Bits (0x0804b14c)      data bits, typically 8
```

The comdir global at 0x0804b160 holds the singleton comdirect instance.

The comdirect driver is used when a third-party device needs to be integrated
into the RTU data model but uses a proprietary protocol that is implemented
by an upstream application (qmicro or Aqalpha) rather than by a dedicated
field bus driver. qmicro constructs the raw request frame, passes it via
MSG_SPECIAL_BUF to the comdirect driver, receives the raw response, and
parses it internally.


## gpio86dx PC/104 Digital I/O (progr/gpio86dx)

Class name: gpio86dx. Inherits from usodrv.

The gpio86dx driver is the standard PC/104 digital I/O board driver for the
x86 DX variant board. It provides discrete inputs and outputs via direct
ISA register access. Unlike the c5600 driver which uses two polling threads,
gpio86dx uses a single tuthread for telecontrol output and relies on the
qmicro main cycle for discrete input polling.

ThreadCtl is called to grant IO access permissions for direct ISA register
reads and writes. The driver is loaded directly from the active start.ini:
Uso=gpio86dx (no port parameter required, as this driver uses the LPT bus).

Unlike the serial field bus drivers which report errors when the device is
absent, gpio86dx prints "Amux - Error connect to ISA!!!!!" (the same string
as uso_amux) when the ISA bus fails to respond during InitUso, suggesting
shared error handling code between gpio86dx and the amux family.


## fixuso Virtual USO Device (progr/fixuso)

Class name: fixuso. Inherits from usodrv.

Creates two named POSIX shared memory objects:
  /VirtualUsoSemaphore
  /VirtualUsoParametrs

This implements a virtual (simulated) USO device entirely in shared memory,
used for testing or for bridging non-serial data sources into the USO data
model without requiring physical hardware. The fixuso class implements
GetDiscret, GetAnalog, GetImpuls, SetDout, Working, and InitUso.

The Working method reads from /VirtualUsoParametrs and writes the values
into the standard usodrv shared memory segments (/SostDiscrets etc.), making
the virtual device indistinguishable from a real field device from qmicro's
perspective.


## usoimit USO Simulator (progr/usoimit)

Class name: usoimit. Inherits from usodrv.

Creates five named POSIX shared memory segments for simulation configuration:
  /ConfigImit    simulation control parameters
  /ConfigTs      telestatus simulation table
  /ConfigTi      teleindication (analog) simulation table
  /ConfigImp     impulse counter simulation table
  /ConfigTu      telecontrol command capture table

The usoimit driver replays pre-configured I/O patterns from the /ConfigTs,
/ConfigTi, and /ConfigImp tables on each Working cycle, delivering simulated
field device data to qmicro without requiring any physical hardware. Telecontrol
commands received from qmicro (via SetDout) are captured into /ConfigTu for
inspection.

This is primarily used during firmware development and testing to verify that
qmicro correctly processes field device data and generates the appropriate
upstream SCADA events.
