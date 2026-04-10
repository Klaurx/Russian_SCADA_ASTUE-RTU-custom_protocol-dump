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
the USOTM validator.

The transport mode is controlled by the FL_NoIp global. When FL_NoIp is clear
and DefaultIp/DefaultPort are configured, the UDP path is used. When FL_NoIp
is set, the RS-232 serial path is used. The default serial port string /dev/ser1
appears in the binary strings section. This means KPRIS is not exclusively
"UDP serial" as earlier documentation suggested; it is RS-232 serial or UDP
depending on runtime configuration.

Two WaitOtvet variants exist in ckpris:
  WaitOtvet at 0x804b690 for the standard response wait path
  WaitOtvet1 at 0x804f7b8 for a secondary wait path used in specific sequences

## Outbound Request Strings

All Zapros format strings are debug log entries printed immediately before
the corresponding outbound frame is transmitted on the wire. They serve as
reliable identifiers of the command type encoded in each request frame.

```
Zapros Ti Kol=%ld
  Request teleindication point count from device.
  The %ld field is the number of TI points expected in the response.
  The device responds with its current TI data point inventory.

Zapros Oscill
  Request oscillogram capture initiation.
  A large stack allocation of 0x1430 bytes (5168 bytes) in SendZaprosOscill
  confirms the oscillogram response buffer size. The oscillogram pipeline
  uses two separate methods: SendZaprosOscill transmits the request and
  MakeZaprosOscill builds the request frame independently, allowing the
  request to be prepared and queued before transmission.

Zapros spisok files
  Request a directory listing of all files stored on the KPRIS device.
  The response contains file names and sizes in device-specific format.

Zapros Read File
  Request a file read operation by name or index.
  The MakeZaprosReadFile method builds the request frame.

Zapros Write File
  Request a file write operation.
  The MakeZaprosWriteFile method builds the request frame.

Zapros Get Info
  Request device identification and firmware version information.
  The MakeZaprosGetInfo method builds the request frame.

Zapros Reboot
  Request device reboot. This is a privileged command that causes the
  KPRIS device to restart its internal processor.
  The MakeZaprosReboot method builds the request frame.

Zapros Delete File
  Request file deletion by name or index on the KPRIS device.
  The MakeZaprosDeleteFile method builds the request frame.

Zapros Spisok Journals
  Request a listing of all available journals (event logs) on the device.
  The MakeZaprosSpisokJournals method builds the request frame.

Zapros Journal
  Request download of a specific event journal.
  The MakeZaprosReadJournal method builds the request frame.

Zapros current Config
  Request the device's currently active running configuration as a data block.
  Two config methods exist: MakeZaprosReadConfig for reading and
  MakeZaprosWriteConfig for writing back a modified configuration.

ZaprosSysInfo=%ld
  Request system information. The numeric argument is logged with %ld format.
  The RaspakSysInfo method parses the response.

Zapros Ti NomUso=%ld
  Request teleindication data for a specific USO node number.
  The node number is embedded in the frame and logged.

Zapros Ts NomUso=%ld
  Request telestatus data for a specific USO node number.
```

## Control Commands

```
SetTu1 KolPriem=%ld
  Send a group telecontrol command to the device.
  KolPriem is the reception acknowledgment count, used for retry tracking.
  Implemented by SetGroupTu(iocuso*, unsigned short, unsigned short).

SetOneDout
  Send a single discrete output command.
  The target USO number is logged as nomuso=%ld.
  Implemented by SetOneDout(iocuso*, unsigned short, unsigned short,
  unsigned short) with four parameters: device, point, value, and duration.

SetOneTu %ld %ld
  Send a single telecontrol command with two numeric parameters logged.
```

## Response and Event Strings

```
Priem KolEvent=%ld
  Received an event buffer from the device.
  KolEvent is the count of events contained in the buffer.
  Parsed by RaspakBufEvent.

Ts Event=%ld
  Received a telestatus event notification.
  The numeric event code is logged.

Error crc = %lx
  CRC mismatch detected on a received frame.
  The hex value is the received CRC word for diagnostic comparison.

Error func %lx %lx %lx
  Unexpected function code received in a response frame.
  Three hex values are logged: the received code and two context values.

Start oscillogramma
  Debug string emitted when oscillogram data capture begins.

Zapros Buf Event NomUso=%ld FL_Kvit=%ld
  Requesting event buffer for a specific USO number.
  FL_Kvit controls whether a kvitok acknowledgment is sent after reception.
```

## Identification and Status Strings

```
IdentMy=%ld Filename=[%s]
  Self-identification with file transfer context.
  Used during file read and write operations to identify the transfer.

IdentBuf=%ld
  Buffer identification number for multi-packet transfers.

Ident Uso=%ld-%ld
  USO identification with two fields separated by a literal dash character
  in the format string itself.

NomUso=%ld AdrUso=%lx KolErr=%ld
  USO poll status line: device number, hex address, and cumulative error count.

Opros uso N %ld Adr=%lx %ld %ld
  Polling USO device N at hex address Adr with two additional status values.

End Opros uso N %ld %ld %ld
  Completed polling USO device N with three status values.

uso N %ld KTI=%lf KTU=%lf
  USO device N with its teleindication and telecontrol scaling coefficients.

Send=%ld bytes
  Debug count of bytes transmitted in a frame.

LenBuf=%ld
  Length of the current communication buffer.

KPR No Memory
  Memory allocation failure in the KPRIS driver. Fatal error condition.

Error open %s
  Failed to open the serial port or file specified in the string argument.

127.0.0.1
  Default fallback IP address used when DefaultIp is not configured.
```

## Frame Layout

The KPRIS protocol frame structure includes an IDENT block (the IDENT struct,
accessed via FindUsoForIdent), a device address byte at tag[+0x39], and a
4-byte identification block at tag[+0x4] copied directly into control frames.

Coefficient scaling: GetKoeff and GetKt provide TI and TU measurement scaling
coefficients from the device definition. The global FL_UsePriborKtiKtu controls
whether device-specific or global coefficients are applied.

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

## Priority Queue System

ckpris maintains a two-level command queue for telecontrol operations:

  AddTuInQueneLow  accepts normal-priority telecontrol commands
  AddTuInQueneHigh accepts urgent high-priority telecontrol commands

This allows emergency telecontrol commands to jump ahead of queued normal
commands when time-critical switching operations are needed at the substation.

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
KolTsEvent1          secondary telestatus event counter
MAX_TEST_POVTOR      maximum retry count before marking device as offline
MAX_TEST_VALUE       maximum value threshold for test comparisons
MAX_ERRORS           error count threshold before USO device is marked offline
mnoj                 multiplier constant for measurement scaling
```
