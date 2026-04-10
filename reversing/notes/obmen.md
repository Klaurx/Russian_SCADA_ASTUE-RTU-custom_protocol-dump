# Obmen() Exchange Loop Analysis

## Function Signature

```
kanaldrv::Obmen() -> void
```

Address in libkanaldrv.so.1: 0x5652

## Purpose

The main poll and exchange loop for the RTOS link layer. Runs as the
WorkProc thread (entry point at WorkProc(void*)). This function is the
top-level state machine for the channel driver and never returns during
normal operation.

## State Machine

The loop body executes in sequence on each iteration.

Step 1: Periodic transmit check.

```
; Check if periodic timer has fired based on modulo of current tick
; if flag[+0x224] set: LockBuffer(4), call send_fn for buf1 at [+0x234]
; if flag[+0x225] set: LockBuffer(4), call send_fn for buf2 at [+0x238]
```

The period divisor is at object offset +0x220. When the timer fires and
the pending flags are set, the corresponding SHM send buffer is transmitted
via the transport-layer SendBuffer vtable method.

Step 2: Idle timeout check.

```
if idle_byte_counter > 10:
    error_counter[+0x97C0]++
```

Step 3: Atomic RX gate.

```
atomic_set(0xABCD) on atom_rx at [+0x97B4]
```

Step 4: Session timeout.

```
if current_time - last_rx_time > timeout_10ms[+0x228]:
    vtable[+0x3C] = TimeReInitKanal()
```

The timeout is in units of 10ms. A value of 0 means no timeout.

Step 5: Read one byte.

```
vtable[+0x20] = ReadByteFromPort(fd, &byte)
if fail:
    delay(10ms)
    idle_byte_counter++
    goto step 3
atom_tx at [+0x97B8] = 0xABCD
idle_byte_counter = 0
```

Step 6: Frame accumulation.

```
PriemPacket(byte, &sost_priem, rx_buf, 0x9000)
if fail:
    fail_counter++
    goto step 3
```

The maximum rx buffer size is 0x9000 bytes (36864). PriemPacket accumulates
bytes into the receive buffer using the SOST_PRIEM FSM.

Step 7: Frame dispatch.

```
result = AnalizBufPriem(&sost_priem, rx_buf, byte, &len, timeout_f, &time_corr, channel)

switch result:
  1:  AnalizBufPriemAndSendOtvet(&retr_adr, channel)
  2:  if ack_mode[+0x226]: SendKvitok(...)
  4:  partial frame, continue accumulating
  default: vtable[+0x2C] = CloseSeans()
```

## Key Object Offsets Referenced

```
+0x224   tx_buf1_pending flag
+0x225   tx_buf2_pending flag
+0x220   period_count divisor
+0x228   timeout_10ms session timeout
+0x226   ack_mode flag (0=none, 1=partial, 2=full)
+0x234   pointer to SHM TX buffer 1
+0x238   pointer to SHM TX buffer 2
+0x97B4  atom_rx atomic flag
+0x97B8  atom_tx atomic flag
+0x97C0  error counter
+0x97AC  pointer to RTOS_RETRANSLATE_ADR
+0x230   current channel address (uint16)
+0x1D0   SOST_PRIEM struct (18 bytes)
+0x1E4   SOST_TIME_CORRECT struct (16 bytes)
```

## Thread Model

WorkProc is spawned by StartDrv(). A separate TestProc handles diagnostics.
WorkInitProc handles the init handshake via the initkandrv subclass.

The initkandrv::InitSend() function (0x48BA) runs a parallel loop that:
  Calls MakeNextSrezBuf to fill the send buffer
  Checks both TX buffer sequence numbers at [+0x234+2] and [+0x238+2]
  If both are zero (nothing pending), delays 200ms
  If the srez buffer at [+0x97CC] is non-zero, calls SendBuffer with the
    current channel address from [+0x230] and unit_id from [+0x4]
  Waits for the srez ring buffer [+0x97CC] to clear to 0 before continuing
