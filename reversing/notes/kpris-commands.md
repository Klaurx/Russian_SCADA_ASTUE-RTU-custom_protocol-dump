# KPRIS Protocol Command Analysis

## Binary

progr/kpris  (49483 bytes)
Source file recovered: kpris.cc
Class name: ckpris (inherits usodrv)

## Command Set (Recovered from Strings)

All Zapros* strings are format strings for debug logging printed before the
corresponding outbound frame is transmitted. They directly identify the
command type encoded in the request frame sent to the KPRIS substation device.

```
Zapros Ti Kol=%ld
  Request teleindication (TI) point count from device.
  %ld is the number of TI points expected in response.

Zapros Oscill
  Request oscillogram capture start.
  Large stack allocation (0x1430 bytes) in SendZaprosOscill confirms the
  oscillogram response buffer is up to 5168 bytes.

Zapros spisok files
  Request directory listing of files on the device.

Zapros Read File
  Request file read by name/index.

Zapros Write File
  Request file write operation.

Zapros Get Info
  Request device identification and version information.

Zapros Reboot
  Request device reboot. This is a privileged command.

Zapros Delete File
  Request file deletion by name/index.

Zapros Spisok Journals
  Request listing of available journals (event logs).

Zapros Journal
  Request download of a specific journal.

Zapros current Config
  Request the device's running configuration.

ZaprosSysInfo=%ld
  Request system information. The numeric argument is logged.

Zapros Ti NomUso=%ld
  Request TI data for a specific USO node number.

Zapros Ts NomUso=%ld
  Request TS (telestatus) data for a specific USO node number.
```

## Control Commands

```
SetTu1 KolPriem=%ld
  Send group telecontrol command. KolPriem is the reception count (retries).

SetOneDout
  Send single discrete output command. nomuso=%ld logged.

SetOneTu %ld %ld
  Send single telecontrol command with two parameters.
  Implemented in ckpris::SetOneDout (P6iocusottt signature: device, point,
  value, duration as uint16 arguments).
```

## Response/Event Strings

```
Priem KolEvent=%ld
  Received event buffer. KolEvent is the count of events in the buffer.

Ts Event=%ld
  Received a TS (telestatus) event. Numeric event code logged.

Error crc = %lx
  CRC mismatch on received frame. The hex value is the received CRC.

Error func %lx %lx %lx
  Unexpected function code in received frame.
```

## Identification Strings

```
IdentMy=%ld Filename=[%s]
  Self-identification with file transfer context.

IdentBuf=%ld
  Buffer identification number.

Ident Uso=%ld-%ld
  USO identification. Two fields separated by dash.

NomUso=%ld AdrUso=%lx KolErr=%ld
  USO poll status: device number, hex address, error count.

Opros uso N %ld Adr=%lx %ld %ld
  Polling USO device N at address Adr.

End Opros uso N %ld %ld %ld
  Completed polling USO N.
```

## Address and Frame Layout Indicators

The KPRIS protocol uses a frame structure that includes:
  An IDENT block (struct IDENT, found as FindUsoForIdent parameter)
  A device address byte at tag[+0x39] (visible in SetGroupTu and SetOneDout)
  A 4-byte identification block at tag[+0x4] copied into control frames
  A coefficient table (GetKoeff, GetKt) for TI/TS scaling

The frame header for group TU (confirmed from SetGroupTu disassembly):

```
Offset   Value          Field
0        0x5B           Start byte
1        0x04           Command type (group telecontrol)
2        tag[+0x39]     Device address
3        0x00
4..7     tag[+0x4..7]   4-byte device identification
8        0x00
9        0x00
10..13   Bitmask encoding (NOT(mask)<<16 | mask)
14       0x01 or 0x00   Activate/deactivate
total    18 bytes (0x12)
```

## Parameters Accepted at Startup

```
Speed       baud rate for serial port
Pause       inter-character timeout in ms
Stopbits    stop bits (1 or 2)
Parity      parity (N/E/O)
Bits        data bits (typically 8)
TypeUso     device type code discriminator
DefaultIp   fallback IP address (UDP transport variant)
DefaultPort fallback port (UDP transport variant)
Port        serial port device path
```

## Global State Flags

```
FL_NoRazdel     disable separator logic
FL_UsePriborKtiKtu  use device-specific KTI/KTU scaling coefficients
FL_NoIp         disable IP (serial-only mode)
fl_debug        enable debug logging
KolTsEvent      count of TS events processed
KolTsEvent1     secondary TS event counter
MAX_TEST_POVTOR maximum retry count
MAX_TEST_VALUE  maximum test value
MAX_ERRORS      error threshold before USO marked offline
mnoj            multiplier constant
```
