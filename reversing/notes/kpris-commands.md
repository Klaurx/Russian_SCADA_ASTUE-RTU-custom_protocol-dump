# KPRIS Protocol Command Analysis

## Binary

progr/kpris, 49483 bytes
Source file recovered from strings: kpris.cc
Class name: ckpris, inherits from usodrv

The ckpris class has its own TestPriem method, distinct from cusotm::TestPriem,
with the signature:

  ckpris::TestPriem(unsigned char, unsigned char*,
                    unsigned short&, unsigned char, unsigned char)

This five-parameter signature differs fundamentally from the two-parameter
cusotm version. KPRIS uses its own frame validation logic and cannot share
the USOTM validator. The five parameters are: a single byte (likely the
frame start byte or command code), the frame buffer pointer, a reference to
the parsed length, a device address byte, and a checksum or mode byte.

The transport mode is controlled by the FL_NoIp global. When FL_NoIp is clear
and DefaultIp/DefaultPort are configured, the UDP path is used. When FL_NoIp
is set, the RS-232 serial path is used. The default serial port string /dev/ser1
appears in the binary strings section. This means KPRIS is not exclusively
a UDP protocol: it operates over RS-232 serial or UDP depending on runtime
configuration.

Two WaitOtvet variants exist in ckpris:
  WaitOtvet at 0x804b690 for the standard response wait path
  WaitOtvet1 at 0x804f7b8 for a secondary wait path used in specific sequences

The KPRIS protocol uses the dev-udp virtual serial device (described in
docs/architecture.md) when operating over UDP, allowing the ckpris driver
to use serial port file descriptors transparently regardless of whether
the underlying transport is RS-232 or UDP.


## Outbound Request Strings

All Zapros format strings are debug log entries printed immediately before
the corresponding outbound frame is transmitted on the wire. They serve as
reliable identifiers of the command type encoded in each request frame.

```
Zapros Ti Kol=%ld
  Request teleindication point count from device.
  The %ld field is the number of TI points expected in the response.
  The device responds with its current TI data point inventory.
  This is the initialisation query sent when first connecting to a KPRIS
  device: the RTU requests the device to report how many measurement points
  it has configured, allowing the RTU to allocate the correct number of
  data slots.

Zapros Ti NomUso=%ld
  Request teleindication data for a specific USO node number.
  The node number %ld identifies which measurement unit within the KPRIS
  device is being polled. A single KPRIS device may contain multiple
  measurement units (substations, feeders, or protection relay groups)
  each with their own USO node number.

Zapros Ts NomUso=%ld
  Request telestatus data for a specific USO node number.
  Same node number addressing scheme as ZaprosTi.

ZaprosSysInfo=%ld
  Request system information from the device.
  The numeric argument is the info type code and is logged with %ld format.
  The RaspakSysInfo method parses the response, which contains device
  firmware version, hardware revision, and operational status.

Zapros Oscill
  Request oscillogram capture initiation.
  An oscillogram is a waveform recording capturing voltage and current
  waveforms at high sample rates during a fault event.
  A large stack allocation of 0x1430 bytes (5168 bytes) in SendZaprosOscill
  confirms the oscillogram response buffer size. The oscillogram pipeline
  uses two separate methods: SendZaprosOscill transmits the request and
  MakeZaprosOscill builds the request frame independently, allowing the
  request to be prepared and queued before transmission.
  Oscillograms are typically recorded automatically on protection relay
  operation and are retrieved by the RTU for upload to the SCADA master.

Zapros spisok files
  Request a directory listing of all files stored on the KPRIS device.
  The response contains file names, sizes, and timestamps in device-specific
  format. Files include oscillograms, event journals, and configuration data.

Zapros Read File
  Request a file read operation by name or index.
  The MakeZaprosReadFile method builds the request frame with the file
  identifier (name string or index number).

Zapros Write File
  Request a file write operation.
  The MakeZaprosWriteFile method builds the request frame.
  Used for configuration updates pushed from the SCADA master to the device.

Zapros Get Info
  Request device identification and firmware version information.
  The MakeZaprosGetInfo method builds the request frame.
  The response contains the device model code, serial number, firmware version,
  and manufacturing date.

Zapros Reboot
  Request device reboot. This is a privileged command that causes the
  KPRIS device to restart its internal processor.
  The MakeZaprosReboot method builds the request frame.
  Used for remote firmware updates or to recover a hung device.
  The RTU must have the appropriate access level to issue this command.

Zapros Delete File
  Request file deletion by name or index on the KPRIS device.
  The MakeZaprosDeleteFile method builds the request frame.
  Used to free storage space after oscillograms have been retrieved.

Zapros Spisok Journals
  Request a listing of all available journals (event logs) on the device.
  The MakeZaprosSpisokJournals method builds the request frame.
  The response lists journal types (protection operation, measurement,
  communication) with their current record counts.

Zapros Journal
  Request download of a specific event journal.
  The MakeZaprosReadJournal method builds the request frame with the journal
  type identifier.

Zapros current Config
  Request the device's currently active running configuration as a data block.
  Two config methods exist: MakeZaprosReadConfig for reading and
  MakeZaprosWriteConfig for writing back a modified configuration.
  The configuration block contains protection relay setpoints, measurement
  transformer ratios, and communication parameters.
```


## Control Commands

```
SetTu1 KolPriem=%ld
  Send a group telecontrol command to the device.
  KolPriem is the reception acknowledgment count, used for retry tracking.
  Implemented by SetGroupTu(iocuso*, unsigned short, unsigned short).
  Group telecontrol commands switch circuit breakers, isolators, or change
  relay protection settings for an entire protection group simultaneously.

SetOneDout
  Send a single discrete output command.
  The target USO number is logged as nomuso=%ld.
  Implemented by SetOneDout(iocuso*, unsigned short, unsigned short,
  unsigned short) with four parameters: device, point, value, and duration.
  Single discrete outputs control individual switchgear elements.

SetOneTu %ld %ld
  Send a single telecontrol command with two numeric parameters logged.
  The two parameters identify the target point and the command value.
```


## Response and Event Strings

```
Priem KolEvent=%ld
  Received an event buffer from the device.
  KolEvent is the count of events contained in the buffer.
  Parsed by RaspakBufEvent.
  Events include protection relay operations, measurement threshold crossings,
  device self-diagnostics, and communication link state changes.

Ts Event=%ld
  Received a telestatus event notification.
  The numeric event code identifies the specific discrete state change.
  Each KPRIS device maintains a timestamped event log that the RTU polls
  periodically and retrieves via ZaprosEvent.

Error crc = %lx
  CRC mismatch detected on a received frame.
  The hex value is the received CRC word for diagnostic comparison against
  the computed CRC, allowing field engineers to identify corrupted frames.

Error func %lx %lx %lx
  Unexpected function code received in a response frame.
  Three hex values are logged: the received function code and two context
  values (likely the expected code and the device address). Used for
  diagnosing protocol mismatches between RTU and device firmware versions.

Start oscillogramma
  Debug string emitted when oscillogram data capture begins.
  The capture involves multiple sequential read operations as the large
  waveform data (5168 bytes per oscillogram) is transferred in chunks.

Zapros Buf Event NomUso=%ld FL_Kvit=%ld
  Requesting event buffer for a specific USO number.
  FL_Kvit controls whether a kvitok acknowledgment is sent after reception.
  When FL_Kvit is set, the device expects an acknowledgment before clearing
  its event buffer; when clear, the buffer is cleared on delivery.
```


## Identification and Status Strings

```
IdentMy=%ld Filename=[%s]
  Self-identification with file transfer context.
  Used during file read and write operations to identify the transfer.
  IdentMy is the RTU's own identification number in the KPRIS network.

IdentBuf=%ld
  Buffer identification number for multi-packet transfers.
  Large files (oscillograms, journals) are transferred in numbered buffers
  to allow resume after communication interruption.

Ident Uso=%ld-%ld
  USO identification with two fields separated by a literal dash character.
  The format is Ident Uso=NomUso-AdrUso showing both the logical USO number
  and its physical address in the KPRIS network.

NomUso=%ld AdrUso=%lx KolErr=%ld
  USO poll status line: device number, hex address, and cumulative error count.
  Printed at the start of each poll cycle for diagnostic purposes.

Opros uso N %ld Adr=%lx %ld %ld
  Polling USO device N at hex address Adr with two additional status values.
  The two additional values are typically the current error count and the
  last successful poll sequence number.

End Opros uso N %ld %ld %ld
  Completed polling USO device N with three status values.
  Printed at the end of each poll cycle to mark completion.

uso N %ld KTI=%lf KTU=%lf
  USO device N with its teleindication and telecontrol scaling coefficients.
  KTI (coefficient for teleindication) scales raw analog counts to engineering
  units (volts, amps, watts). KTU (coefficient for telecontrol) scales the
  command values. These coefficients are read from the device configuration
  via GetKoeff and GetKt.

Send=%ld bytes
  Debug count of bytes transmitted in a frame. Aids in verifying that
  the full frame was transmitted without truncation.

LenBuf=%ld
  Length of the current communication buffer.

KPR No Memory
  Memory allocation failure in the KPRIS driver. Fatal error condition
  indicating malloc returned null for an oscillogram or journal buffer.

Error open %s
  Failed to open the serial port or virtual serial device specified in the
  string argument. The %s is the full device path (/dev/ser1 or /dev/serudpN).

127.0.0.1
  Default fallback IP address used when DefaultIp is not configured.
  This localhost address is used during development and testing when
  the KPRIS device is replaced by a software simulator on the same machine.
```


## Frame Layout

The KPRIS protocol frame structure includes an IDENT block (the IDENT struct,
accessed via FindUsoForIdent), a device address byte at tag[+0x39], and a
4-byte identification block at tag[+0x4] copied directly into control frames.

Coefficient scaling: GetKoeff and GetKt provide TI and TU measurement scaling
coefficients from the device definition. The global FL_UsePriborKtiKtu controls
whether device-specific or global coefficients are applied. When FL_UsePriborKtiKtu
is set, each device's own KTI and KTU values (stored in its DEF_USO record)
are used. When clear, the global coefficients from the mnoj multiplier are used.

Group TU frame header, confirmed from SetGroupTu disassembly:

```
Offset   Value             Description
0        0x5b              Start byte
1        0x04              Command type, group telecontrol
2        tag[+0x39]        Device address byte
3        0x00              Reserved
4..7     tag[+0x4..7]      4-byte device identification block
8        0x00              Reserved
9        0x00              Reserved
10..13   Bitmask encoding  NOT(mask) left-shifted 16 bits OR mask, 4 bytes
14       0x01 or 0x00      Activate (1) or deactivate (0) flag
Total    18 bytes
```

The bitmask encoding at bytes 10 through 13 encodes both the active bits
(mask) and their complements (NOT(mask)) in a single 32-bit word. The lower
16 bits are the activation bitmask and the upper 16 bits are the bitwise
complement of the same mask. This dual encoding allows the receiving device
to verify that the command was received without corruption by checking that
the upper and lower halves are exact complements.


## Priority Queue System

ckpris maintains a two-level command queue for telecontrol operations:

  AddTuInQueneLow accepts normal-priority telecontrol commands.
  AddTuInQueneHigh accepts urgent high-priority telecontrol commands.

This allows emergency telecontrol commands (such as protective relay trip
commands during a fault) to jump ahead of queued normal commands (such as
scheduled switching operations) when time-critical actions are needed.
High-priority commands are delivered immediately on the next available
transmission slot regardless of queued normal commands.

The oscillogram request pipeline uses SendZaprosOscill and MakeZaprosOscill
as distinct stages. MakeZaprosOscill builds the request frame and stores it
in an intermediate buffer. SendZaprosOscill takes the pre-built frame and
schedules its transmission. This separation allows the RTU to prepare the
oscillogram request during the preceding poll cycle and have it ready for
immediate transmission when the oscillogram data becomes available.


## Startup Parameters Accepted

```
Speed       baud rate for the RS-232 serial port
Pause       inter-character gap timeout in milliseconds
Stopbits    number of stop bits, 1 or 2
Parity      parity setting: N for none, E for even, O for odd
Bits        number of data bits, typically 8
TypeUso     device type code discriminator for multi-type deployments
DefaultIp   fallback IP address for the UDP transport variant
DefaultPort fallback port number for the UDP transport variant
Port        serial port device path
```


## Global State Variables

```
FL_NoRazdel          disable separator logic in frame parsing
FL_UsePriborKtiKtu   use device-specific KTI and KTU scaling coefficients
FL_NoIp              disable IP transport and use serial-only mode
fl_debug             enable verbose debug logging to console
KolTsEvent           running count of telestatus events processed
KolTsEvent1          secondary telestatus event counter for a second
                     event class (likely protection relay operation events
                     counted separately from measurement threshold events)
MAX_TEST_POVTOR      maximum retry count before marking device as offline
MAX_TEST_VALUE       maximum value threshold for test comparisons
MAX_ERRORS           error count threshold before USO device is marked offline
mnoj                 multiplier constant for measurement scaling applied
                     when FL_UsePriborKtiKtu is clear
```
