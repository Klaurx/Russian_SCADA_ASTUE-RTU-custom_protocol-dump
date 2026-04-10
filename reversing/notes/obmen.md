# Obmen() Exchange Loop Analysis

## Function Signature

```
kanaldrv::Obmen() -> void
```

Address in libkanaldrv.so.1: 0x5652

## Purpose

The Obmen method is the main poll and exchange loop for the RTOS link layer.
It runs as the WorkProc thread, with the entry point at WorkProc(void*).
This function is the top-level state machine for the channel driver and never
returns during normal operation. It implements a receive-driven loop that
handles periodic transmit scheduling, session timeout, and frame dispatch.

## Thread Model

WorkProc is spawned by StartDrv() as the primary working thread. A separate
TestProc handles diagnostics and statistics. WorkInitProc handles the initial
handshake sequence via the initkandrv subclass.

The initkandrv subclass provides the init-phase implementation:
  ErrorInitKanal   called on initialisation failure
  CloseInitKanal   closes the initialisation channel
  AnalizBufInitPriem  analyses frames received during initialisation
  PriemKvitEvent   receives a kvitok acknowledgment during init

The InitSend() method of initkandrv (at address 0x48ba) runs in parallel
with WorkProc during the startup phase. It calls MakeNextSrezBuf to fill
the transmit buffer, checks both TX buffer sequence numbers at [+0x234+2]
and [+0x238+2], and delays 200ms if both are zero (nothing pending). When
the srez ring buffer at [+0x97cc] is non-zero, it calls SendBuffer with
the current channel address from [+0x230] and unit_id from [+0x4]. It waits
for the ring buffer to clear to zero before continuing.

## State Machine

The loop body executes the following steps in sequence on each iteration.

### Step 1: Periodic Transmit Check

The period divisor is stored at object offset +0x220. When the periodic
timer fires based on a modulo comparison of the current system tick:

  If flag at [+0x224] is set, LockBuffer(4) is called and the send function
    for TX buffer 1, pointed to at [+0x234], is invoked via the transport
    layer SendBuffer vtable method.

  If flag at [+0x225] is set instead, TX buffer 2 at [+0x238] is transmitted.

The two flags allow independent scheduling of two separate transmit queues,
one for normal srez data and one for retranslation or secondary channel data.

### Step 2: Idle Timeout Check

```
if idle_byte_counter > 10:
    error_counter at [+0x97c0] is incremented by 1
```

This tracks periods where the channel is receiving no data at all, which may
indicate a lost connection or powered-down remote device.

### Step 3: Atomic RX Gate

```
atomic_set(0xABCD) on atom_rx at [+0x97b4]
```

This sets a sentinel value into the receive activity flag. The value 0xABCD
serves as a recognisable marker that can be checked by the diagnostics thread
to verify the receive loop is active.

### Step 4: Session Timeout

```
if current_time minus last_rx_time > timeout_10ms at [+0x228]:
    vtable[+0x3c] = TimeReInitKanal()
```

The timeout is stored in units of 10 milliseconds. A value of zero means no
timeout is applied. TimeReInitKanal() is implemented as a stub returning void
in the kanaldrv base class; each transport subclass overrides it to perform
transport-specific session reset (for example, closing and reopening a TCP
connection in ctcpqnx).

### Step 5: Read One Byte

```
result = vtable[+0x20] ReadByteFromPort(fd, byte_buffer)
if result indicates failure:
    delay(10ms)
    idle_byte_counter++
    goto step 3
atom_tx at [+0x97b8] = 0xABCD
idle_byte_counter = 0
```

ReadByteFromPort is a virtual method, implemented differently for each
transport. For ctcpqnx it calls recv() on the TCP socket with a select()
timeout. For csercom it reads directly from the serial port file descriptor.

### Step 6: Frame Accumulation

```
result = PriemPacket(byte, sost_priem_at_obj_plus_0x1d0,
                     rx_buf, 0x9000)
if result indicates failure:
    fail_counter++
    goto step 3
```

PriemPacket is also a virtual method, implemented in libsystypes.so.1 as
_PriemPacket and called via the vtable slot at [+0x128]. The maximum receive
buffer size is 0x9000 bytes (36864). PriemPacket accumulates bytes into the
receive buffer using the SOST_PRIEM FSM state machine. The SOST_PRIEM struct
at object offset +0x1d0 is 18 bytes (0x12), confirmed from the constructor
zeroing sequence.

### Step 7: Frame Dispatch

```
result = AnalizBufPriem(sost_priem, rx_buf, byte, len_ptr,
                         timeout_float, time_corr_at_obj_plus_0x1e4,
                         channel_uint16)

switch result:
  1:   AnalizBufPriemAndSendOtvet(retr_adr_ptr, channel) handles the
         complete frame and generates an outbound response
  2:   if ack_mode at [+0x226] is non-zero: SendKvitok(...)
  4:   partial frame received, continue accumulating bytes
  other: vtable[+0x2c] CloseSeans() closes the session
```

AnalizBufPriem is implemented in libkanaldrv.so.1 at offset 0x2566 in the
base class. The base class implementation is a pure vtable dispatch:

```
0x25a1:  ff 90 5c 01 00 00  call DWORD PTR [eax+0x15c]
```

This delegates to the subclass implementation via vtable offset 0x15c. The
subclass (e.g. ctcpqnx via initkandrv) provides the actual frame analysis.

Similarly, ReadByteFromPort at offset 0x25b0 delegates via [eax+0x124], and
PriemPacket at offset 0x25ce delegates via [eax+0x128].

## Key Object Offsets

```
+0x4      unit_id, uint16, parsed from a command-line parameter
+0x220    period_count divisor for the periodic timer
+0x224    tx_buf1_pending flag, set when buffer 1 needs transmission
+0x225    tx_buf2_pending flag, set when buffer 2 needs transmission
+0x226    ack_mode flag, 0 means no ack, 1 means partial, 2 means full
+0x228    timeout_10ms, session inactivity timeout in 10ms units
+0x230    current channel address, uint16
+0x234    pointer to shared memory TX buffer 1
+0x238    pointer to shared memory TX buffer 2
+0x1d0    SOST_PRIEM struct, 18 bytes, receive FSM state
+0x1e4    SOST_TIME_CORRECT struct, 16 bytes, GPS time correction state
+0x97b4   atom_rx, atomic RX activity flag, set to 0xABCD each iteration
+0x97b8   atom_tx, atomic TX activity flag, set to 0xABCD after byte receipt
+0x97c0   error counter, incremented when idle_byte_counter exceeds 10
+0x97ac   pointer to RTOS_RETRANSLATE_ADR for the current session
+0x97cc   srez ring buffer pointer, checked by initkandrv::InitSend
```

## SOST_TIME_CORRECT Structure

The SOST_TIME_CORRECT struct at object offset +0x1e4 is 16 bytes and is
passed to AnalizBufPriem and to IsTimeCorrectAllow. Its estimated layout:

```
+0x0   correction_ms    int32, time offset from the GPS reference
+0x4   enabled          uint8, correction enabled flag
+0x5   valid            uint8, correction valid flag
+0x6   padding          uint16
+0x8   last_sync_ms     uint32, timestamp of last GPS synchronisation
+0xc   drift_ppb        uint16, measured clock drift in parts per billion
+0xe   padding          uint16
```

## PrepareBufRetr Delegation

The base class kanaldrv::PrepareBufRetr at offset 0x26c2 delegates to the
subclass via vtable offset 0x18c:

```
0x26ef:  ff 90 8c 01 00 00  call DWORD PTR [eax+0x18c]
```

The first argument is forced to zero (0x0) in the base class wrapper:

```
0x26e5:  c7 04 24 00 00 00 00  mov DWORD PTR [esp], 0x0
```

This pattern of vtable delegation with fixed argument overrides is consistent
across several base class methods in kanaldrv.
