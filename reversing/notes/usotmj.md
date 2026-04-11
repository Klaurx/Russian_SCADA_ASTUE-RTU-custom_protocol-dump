# usotmj SOE Journal Extension Analysis (progr/usotmj)

## Binary Identity

Binary name: usotmj. Class name: cusotmj. Source file: usotmj.cc.
Base class: cusotm (which itself subclasses usodrv).
Build date: December 9, 2024, consistent with the main firmware release.

The cusotmj class is a direct extension of cusotm and shares its serial port
hardware interface, termios configuration, frame format, CRC algorithm, and
all of the base polling functions. The extension adds a full SOE (sequence
of events) journaling layer on top of the base cyclic polling model.

Both usotm and usotmj can be deployed for the same physical field bus. The
choice between them is made in the start.ini Uso= line. The project
configuration in project/start.ini uses usotm. A deployment that requires
timestamped event capture with sub-second resolution would use usotmj instead.


## What SOE Journaling Means in This Context

In the base cusotm driver, each polling cycle reads the current state of all
discrete and analog points and writes the values into the shared memory
segments /SostDiscrets and /SostAnalogs. Change detection is performed by
qmicro comparing the new values against previously stored values. The
timestamp assigned to any detected change is the time of the poll cycle that
detected it, which may be up to one full poll period after the actual event.

In the cusotmj extension, change-driven polling replaces cyclic polling for
telestatus and teleindication. Instead of reading the current state on each
cycle, the driver requests from the field device a list of changes that have
occurred since the last acknowledged change, each carrying a precise device-
side timestamp. This allows sub-second event timestamping even when the poll
cycle period is several seconds.

The journal layer stores received events in an internal ring buffer keyed
by device index and sequence number. qmicro reads from this journal via the
GetDiscretFromJournal and GetAnalogFromJournal methods rather than from the
shared memory state segments, and passes the timestamped events to libjevent.so
for permanent storage.


## TypeUsoJournal Global

The TypeUsoJournal global at address 0x0804fa82 (confirmed from nm, type D,
initialised data) is the single most important difference between usotmj and
usotm at the binary interface level. It is a device type discriminator that
qmicro checks at driver load time to determine whether the loaded USO driver
supports the journal extension interface. If TypeUsoJournal is non-zero,
qmicro uses the journal-capable method dispatch for that driver. If it is
zero (as in usotm), qmicro uses the standard cyclic polling dispatch.

TypeUso at 0x0804fa80 is present in both usotm and usotmj and identifies the
base protocol family. TypeUsoJournal is the additional flag that enables the
extended journal API.


## Method Table

All methods confirmed from nm of the usotmj binary. Methods shared with
cusotm are listed briefly; extended methods are documented in detail.

Methods inherited from cusotm (confirmed present in usotmj):

```
cusotm::AddDoutInQuery(unsigned char, unsigned short, unsigned char,
                       unsigned short)
cusotm::AddError(int)
cusotm::AddKod(int, int, int)
cusotm::AddOK(int)
cusotm::AddUserDataAnalog(...)
cusotm::AddUserDataDiscret(...)
cusotm::AddUserDataImpuls(...)
cusotm::AddUserDataUso(...)
cusotm::GetAddAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
cusotm::GetAnalog(MSG_GET_PARAM*, MSG_RETURN_ANALOG*)
cusotm::GetDiscret(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
cusotm::GetImpuls(MSG_GET_PARAM*, MSG_RETURN_IMPULS*)
cusotm::GetSostUso(unsigned short)
cusotm::GetUkdValue(KEY_INFO*, unsigned char)
cusotm::GetUsomTag(iocuso*)
cusotm::InitPort()
cusotm::IsFirstByte(unsigned char)
cusotm::KolBits(unsigned short)
cusotm::MakeKeyInfo(KEY_INFO*, unsigned char*)
cusotm::MakeWord(unsigned short, unsigned short)
cusotm::RaspakAddAnalog(int, unsigned char*, unsigned short)
cusotm::RaspakAnalog(int, unsigned char*, unsigned short)
cusotm::RaspakDiscret(int, unsigned char*, unsigned short, int)
  note: this version takes an extra int parameter compared to the base
  cusotm::RaspakDiscret which takes only (int, unsigned char*, unsigned short)
cusotm::RaspakImpuls(int, unsigned char*, unsigned short)
cusotm::RaspakKvitUkd(int, unsigned char*, unsigned short)
cusotm::RaspakOldAnalog(int, unsigned char*, unsigned short)
cusotm::RaspakTempNar(int, unsigned char*, unsigned short)
cusotm::RaspakTempVn(int, unsigned char*, unsigned short)
cusotm::RaspakUkd(int, unsigned char*, unsigned short)
cusotm::SendBuffer(unsigned char*, unsigned short)
cusotm::SendSbrosLatch(int)
cusotm::SendSinhroTime()
cusotm::SendTuCommand(unsigned char, unsigned short, unsigned char,
                      unsigned short)
cusotm::SendTuFromQuery()
cusotm::SendTu(int)
cusotm::SendZaprosAddAnalog(int)
cusotm::SendZaprosAnalog(int)
cusotm::SendZaprosImpuls(int)
cusotm::SendZaprosTempNar(int)
cusotm::SendZaprosTempVn(int)
cusotm::SendZaprosUkd(int)
cusotm::SetDout(MSG_SET_DOUT*, MSG_RETURN_DOUT*)
cusotm::SetGroupTu(int)
cusotm::TestPriem(unsigned char*, unsigned short)
cusotm::WaitOtvet(unsigned char*, unsigned short)
cusotm::Working()
```

Methods exclusive to cusotmj (the SOE journal extensions):

```
cusotm::AddEventTiInBuffer(int, unsigned char*, unsigned short*,
                            TIME_SERVER*)
  accumulates a teleindication change event into the internal journal buffer
  parameters:
    int          device slot index
    unsigned char*   the raw frame buffer containing the TI change data
    unsigned short*  pointer to the current frame offset counter (updated
                     in place as the function consumes bytes from the buffer)
    TIME_SERVER*     the timestamp to associate with this event, taken from
                     the GPS time correction path
  for each active TI point in the bitmask, creates a TAG_TI_EVENT record
  containing the point index, old value, new value, and timestamp
  stores the record in the per-device journal ring buffer indexed by device
  slot, advancing the write pointer and wrapping when the buffer is full
  the frame offset pointer is updated so the caller knows how many bytes
  were consumed from the raw frame

cusotm::AddEventTsInBuffer(int, unsigned char, TIME_SERVER*)
  accumulates a telestatus change event
  parameters:
    int          device slot index
    unsigned char    the new telestatus byte value
    TIME_SERVER*     timestamp
  creates a telestatus event record and stores it in the per-device journal
  ring buffer alongside the TI events

cusotm::AddTiEvent(TAG_TI_EVENT*, unsigned char, unsigned char,
                   unsigned short, TIME_SERVER*)
  creates a single TAG_TI_EVENT record from component fields
  parameters:
    TAG_TI_EVENT*   output record to populate
    unsigned char   point type or quality byte
    unsigned char   value byte (new state of the TI point)
    unsigned short  point index or address
    TIME_SERVER*    timestamp for this specific event
  populates all fields of the TAG_TI_EVENT struct and returns

cusotm::DeleteEventTs(int)
  removes a telestatus event from the journal for the device at int index
  called after the event has been successfully acknowledged upstream via
  RaspakTimeKvitok, freeing the journal slot for new events
  operates on the telestatus event buffer rather than the TI buffer

cusotm::GetAnalogFromJournal(TAG_TI_EVENT*, unsigned char, MSG_RETURN_ANALOG*)
  retrieves a historical analog value from the journal
  parameters:
    TAG_TI_EVENT*    the event record to retrieve the value from
    unsigned char    the channel index within the event record
    MSG_RETURN_ANALOG*   output struct to populate
  converts the raw value stored in the TAG_TI_EVENT into engineering units
  and populates MSG_RETURN_ANALOG with the converted value and timestamp
  called by qmicro when it processes the journal rather than the live state

cusotm::GetDiscretFromJournal(MSG_GET_PARAM*, MSG_RETURN_DISCRET*)
  retrieves a historical discrete state from the journal
  parameters:
    MSG_GET_PARAM*     identifies which device and point to retrieve
    MSG_RETURN_DISCRET*  output struct to populate
  searches the telestatus event buffer for events matching the query
  parameters, returns the most recent matching event
  if no matching event is found, falls back to the live state from shared
  memory (same as the base cusotm::GetDiscret behaviour)

cusotm::IsTsWriteEvent(int, unsigned char)
  checks whether a specific telestatus point is configured to generate
  journal events
  parameters:
    int          device slot index
    unsigned char    telestatus point index within the device
  reads the event enable bitmask for the device and checks the bit
  corresponding to the point index
  returns non-zero if the point should generate journal entries, zero if
  it is configured for cyclic polling only

cusotm::MakeZaprosChangeTi(int, int, unsigned char*)
  builds an outbound change-request frame for teleindication
  parameters:
    int          device slot index
    int          starting sequence number for the change request
    unsigned char*   output buffer for the built frame
  constructs a frame requesting all TI changes since the given sequence
  number, allowing the driver to catch up after a communication interruption
  the sequence number is stored in the field device and incremented on each
  acknowledged change event

cusotm::MakeZaprosChangeTs(int, unsigned char*)
  builds an outbound change-request frame for telestatus
  parameters:
    int          device slot index
    unsigned char*   output buffer for the built frame
  constructs a frame requesting all TS changes since the last acknowledged
  event, using a sequence counter maintained per device slot

cusotm::MakeZaprosTimeKvitok(int, unsigned char*)
  builds a time-tagged acknowledgment frame
  parameters:
    int          device slot index
    unsigned char*   output buffer for the built frame
  the time-tagged kvitok carries the timestamp of the last received event
  to allow the field device to synchronize its sequence counter with the
  RTU's confirmed receive position
  this is distinct from the basic SendSbrosLatch latch reset: it carries
  full timestamp information rather than just a reset pulse

cusotm::RaspakChangeTi(int, unsigned char*, unsigned short)
  parses a teleindication change response frame
  parameters:
    int          device slot index
    unsigned char*   received frame buffer
    unsigned short   frame length
  after TestPriem validation, extracts the event bitmask and calls
  AddEventTiInBuffer for each changed point, associating the current GPS
  timestamp with all changes in this response
  after processing all changes, calls MakeZaprosTimeKvitok and sends the
  time-tagged acknowledgment back to the field device
  returns non-zero on successful parse, zero on error

cusotm::RaspakChangeTs(int, unsigned char*, unsigned short)
  parses a telestatus change response frame
  same structure as RaspakChangeTi but for telestatus points
  calls AddEventTsInBuffer for each changed telestatus bit
  sends time-tagged acknowledgment on success

cusotm::RaspakTimeKvitok(int, unsigned char*, unsigned short, TIME_SERVER*)
  parses the time-tagged acknowledgment from the field device
  parameters:
    int          device slot index
    unsigned char*   received frame buffer
    unsigned short   frame length
    TIME_SERVER*     output for the timestamp carried in the kvitok
  extracts the timestamp from the field device's acknowledgment, allowing
  the RTU to verify that the field device has received and accepted the
  last transmitted acknowledgment
  on success, calls DeleteEventTs to free the acknowledged events from the
  journal buffer

cusotm::ReadJournalTi(int)
  reads the teleindication journal for the device at index int
  called by qmicro's event processing loop rather than by the Working thread
  retrieves all pending TAG_TI_EVENT records from the per-device ring buffer
  and passes them to AddTsInJournal via the libjevent event logging path
  advances the read pointer after delivery

cusotm::ReadJournalTs(int)
  reads the telestatus journal for the device at index int
  same pattern as ReadJournalTi but for the telestatus event buffer
```


## SendZaprosDiscret Signature Difference

Base cusotm signature at 0x0804c4c2:
  cusotm::SendZaprosDiscret(int)
  one parameter: the device slot index

usotmj signature at 0x0804d0ba:
  cusotm::SendZaprosDiscret(int, int)
  two parameters: device slot index and a mode selector

The second int parameter in the usotmj version controls whether the request
is for full state synchronisation (mode=0, send a standard discrete poll
request identical to the base cusotm version) or for incremental change
reporting (mode=1, send a MakeZaprosChangeTs frame to request only changes
since the last acknowledged event).

This mode selector allows the Working loop in usotmj to seamlessly switch
between full-state polling (used after a communication interruption to
resynchronize) and change-driven polling (used during normal operation to
minimize bandwidth and achieve sub-second event timestamping).

The two binaries are not drop-in replacements for each other. Any code that
calls SendZaprosDiscret on a cusotmj object must pass the second parameter.
The start.ini Uso= line selects which binary is loaded, and qmicro's internal
method dispatch is configured at load time based on TypeUsoJournal.


## __SS__ Session State Global

The __SS__ global at address 0x0804fea8 (confirmed from nm, type D,
initialised data, symbol name with double underscore prefix indicating it
is a compiler or linker-generated state variable) is unique to usotmj and
absent from usotm.

In the usotmj context, __SS__ tracks the session-level acknowledgment state
across multiple change-driven poll cycles. Its value determines whether the
next Working loop iteration should send a new change request or wait for the
field device to acknowledge the previous one. The double-underscore prefix
is unusual for application-level globals and may indicate this is a
compiler-generated static initializer guard variable related to the journal
state machine initialization.


## Additional Imports Unique to usotmj

Confirmed from the undefined symbol table in the usotmj nm output:

```
usodrv::WaitKanalFree(int, int)
  present in usotmj, absent from usotm
  coordinates serial port access across multiple concurrent polling threads
  the base cusotm uses a simpler single-threaded model without port locking
  usotmj needs port locking because the journal write thread and the poll
  thread may both need serial port access simultaneously

usodrv::FreePort(int, int, int)
  present in usotmj, absent from usotm
  releases the serial port lock after a port access sequence completes

usodrv::GetParValue(unsigned short, unsigned short, DEF_PAR_VALUE*)
  present in usotmj, absent from usotm
  reads device configuration parameter values from the parameter table
  used by the journal extension to read event enable bitmasks and
  sequence counter initial values from the device configuration

tcdrain (C library function)
  present in usotmj (confirmed from the C library imports), absent from usotm
  ensures all queued output bytes have been physically transmitted before
  the driver waits for a response
  the journal variant needs this because the time-tagged acknowledgment
  frames must be fully transmitted before the driver can issue the next
  change request, to avoid sequence number ambiguity
```


## Working Loop Changes

The Working() method in usotmj follows the same 11-step structure as the
base cusotm Working() method (documented in docs/protocol.md) but with two
modifications:

Step 11 (discrete poll) calls SendZaprosDiscret(i, mode) with the additional
mode parameter instead of the base SendZaprosDiscret(i). The mode is
determined by the current acknowledgment state tracked in the per-device
journal state.

After RaspakDiscret succeeds, instead of (or in addition to) the standard
SendSbrosLatch call, usotmj calls RaspakChangeTi or RaspakChangeTs depending
on whether the response contained change events or a full-state snapshot.
If the response was change-driven, the time-tagged acknowledgment path is
followed. If it was a full-state poll, the standard latch reset path is used.

The Working loop in usotmj also calls ReadJournalTi and ReadJournalTs
periodically to deliver accumulated journal events to qmicro's event
processing path, ensuring the journal does not overflow during periods of
high change activity.
