# Support Library Analysis

## librashet.so Engineering Calculation Engine

The librashet.so library is the engineering calculation engine loaded by
qmicro at startup via dlopen as specified in the [Module] section of
start.ini. The entry point is InitModule(), which initialises the library's
internal state and registers its function table with libservdrv.so.1 via
LoadServDrv().

The library provides all engineering calculations performed on raw field
device data before the results are transmitted upstream. This includes
deadband filtering, group relay analysis, timer management, formula
evaluation, and all arithmetic operations on I/O point values.

The internal label symbols from .L442 through .L756 (spanning over 300
labels) in the nm output indicate that the RashetAnalog, RashetDiscret,
GroupAnaliz, GroupAnaliz1, GroupAnaliz2, TestRash, and TestRash1 functions
are extremely large with extensive branching. Each formula type in the
start.ini [Formula] section maps to a branch within RashetAnalog or
RashetDiscret.

The BufIzm global (measurement buffer) at 0x0000cca8 and the Config global
at 0x0000ce9c hold the library's internal measurement state and configuration.
The NomMod global at 0x0000cebc is the module slot number assigned by qmicro
when the library is loaded.

All exports confirmed from nm:

Signal processing:

```
TestGran(long, long, long)
  tests whether a value falls outside upper or lower limits (granules)
  returns non-zero if the value exceeds either limit

TestIn(long, long, long)
  tests whether a value falls within a range
  inverse of TestGran

TestMaskBit(int, unsigned char*)
  tests a specific bit in a bitmask byte array
  used for point quality flag checking

TestBit(int, DEF_PAR_VALUE*, unsigned short*, unsigned short*)
  tests a configuration bit parameter and updates quality flags

TestChange(int, unsigned int, unsigned int, DEF_PAR_VALUE*,
           unsigned short*, unsigned short*)
  detects whether a value has changed beyond the configured deadband
  updates the change-pending flag in the quality word

vfabs(double)
  absolute value of a double precision float
  wraps the standard fabs but avoids linking against libm.so.2

SQRT(double)
  square root via the x87 FSQRT instruction
  used in RMS calculations for AC measurements
```

Timer management:

```
InitTimer(int, unsigned char*)
  initialises a simple countdown timer
  int: timer slot index
  unsigned char*: pointer to the timer configuration byte

NextStepTimer(int, unsigned char*)
  advances a timer by one step and checks for expiry
  returns non-zero if the timer has expired

InitTimerZ(int, unsigned char*, DEF_PAR_VALUE*, DEF_PAR_VALUE*)
  initialises a timer with dual configuration parameters
  supports timers that have separate start and stop thresholds

NextStepTimerZ(int, unsigned char*, DEF_PAR_VALUE*, DEF_PAR_VALUE*)
  advances a dual-threshold timer and checks expiry

InitPulsTimer(int, unsigned char*)
  initialises a pulse output timer for generating timed relay pulses

NextStepPulsTimer(int, unsigned char*)
  advances a pulse timer and returns the current pulse state
  (on or off) based on the elapsed time

InitShimTimer(int, unsigned char*, unsigned char)
  initialises a shimmer (rapid on-off) timer for alarm indication
  the unsigned char mode parameter selects the shimmer frequency

NextStepShimTimer(int, unsigned char*)
  advances a shimmer timer and returns the current output state

ClearShimTimer(int, unsigned char*, unsigned char)
  resets a shimmer timer to its initial state

GetDelayTimer(int)
  returns the remaining delay count for timer slot int

GetDlitTimer(int)
  returns the total configured duration for timer slot int

KorrectTime(int)
  applies a correction to the system time based on GPS input
  int: correction amount in milliseconds

NetxStepOtchet(int, unsigned int)
  advances a reporting period counter
  int: period slot index, unsigned int: current time value
```

Relay analysis:

```
ReadSostReley(int, unsigned short*, unsigned short*, unsigned short*,
              unsigned short*, unsigned short*, DEF_PAR_VALUE*,
              unsigned short*, unsigned short*)
  reads the complete relay state for slot int
  populates multiple output words: contact state, quality flags,
  threshold exceeded flags, timing flags, and relay status

GroupAnaliz(int, unsigned int, unsigned int, unsigned int,
            DEF_PAR_VALUE*, unsigned short*, unsigned short*)
  analyses a group of discrete points for collective alarm conditions
  int: group slot index
  unsigned int: current point states (bitmask)
  unsigned int: quality bitmask
  unsigned int: previous states for change detection
  DEF_PAR_VALUE*: group configuration parameters
  unsigned short*: output alarm state
  unsigned short*: output quality flags

GroupAnaliz1(int, unsigned int, unsigned int, unsigned int,
             DEF_PAR_VALUE*, unsigned short*, unsigned short*)
  variant of GroupAnaliz with a different combination logic
  used when the group alarm requires all points in alarm (AND logic)
  vs GroupAnaliz which uses any-point-in-alarm (OR logic)

GroupAnaliz2(int, unsigned int, unsigned int, unsigned int,
             DEF_PAR_VALUE*, unsigned short*, unsigned short*)
  second variant implementing weighted or prioritised group analysis
  some points in the group may be configured as non-contributing to the
  group alarm state, and their quality flags are handled differently
```

Value management:

```
SetAnalog(int, float)
  sets an analog point value in the shared memory segment
  int: point slot index, float: engineering unit value
  called by RashetAnalog after formula evaluation

SetPrValueRA(int, long)
  sets a primary raw ADC value for an analog calculation slot
  int: slot, long: raw value

SetValueRA(int, float)
  sets a secondary engineering unit value for an analog slot

SbrosAnalog(int)
  resets an analog point to its zero or fault state
  called when the field device reports a quality fault

GetValueWithFirst(unsigned short, unsigned short, float*, float*,
                  float*, unsigned short)
  retrieves a value along with its first-derivative estimate
  used for rate-of-change calculations and differential relay functions

ReconfigAnalog(unsigned short)
  reconfigures an analog point's scaling or range parameters
  called when the engineering constants are updated at runtime

ReconfigDiscret(unsigned short)
  reconfigures a discrete point's inversion or quality parameters

ReconfigDout(unsigned short)
  reconfigures a discrete output point's parameters

ReconfigImpuls(unsigned short)
  reconfigures an impulse counter point's scaling parameters
```

Calculation engine:

```
RashetAnalog(void*, void*)
  applies a formula to compute an analog output value from input values
  void* args are DEF_FORMULA and SOST_FORMULA pointers
  the formula can reference any combination of measured analog values,
  engineering constants, and calculated intermediate values
  result is stored via SetAnalog into the appropriate shared memory slot
  this function contains the bulk of the label symbols .L442 through .L756

RashetDiscret(void*, void*)
  applies a formula to compute a discrete output state from input values
  same structure as RashetAnalog but for discrete outputs
  result is stored via a discrete-specific shared memory write

TestRash(int, DEF_PAR_VALUE*, DEF_PAR_VALUE*, DEF_PAR_VALUE*,
         EXT_DEF_BYTE*, unsigned short*, unsigned short*)
  tests a calculation result against configured limits
  int: calculation slot index
  three DEF_PAR_VALUE*: upper limit, lower limit, threshold parameters
  EXT_DEF_BYTE*: extended quality and mode configuration byte
  two unsigned short*: output alarm and quality flags

TestRash1(int, DEF_PAR_VALUE*, DEF_PAR_VALUE*, DEF_PAR_VALUE*,
          EXT_DEF_BYTE*, unsigned short*, unsigned short*)
  variant of TestRash with different hysteresis behaviour
  TestRash uses separate upper and lower threshold comparisons
  TestRash1 uses a single midpoint comparison with configurable hysteresis

TestTime(int, DEF_PAR_VALUE*, unsigned short*, unsigned short*)
  tests a timing condition in a formula evaluation
  int: timer slot index
  DEF_PAR_VALUE*: timing configuration
  two unsigned short*: output state and quality flags

UpdateFlags(DEF_PAR_VALUE*, DEF_PAR_VALUE*, unsigned short*, unsigned short*)
  updates quality and alarm flags based on two parameter comparisons
  called after formula evaluation to propagate quality information
```

Internal shared memory write operations (underscore-prefixed, lower-level):

```
_SetAnalogValue(unsigned short, float)
  direct shared memory write for analog point at slot unsigned short
  writes the float value to /SostAnalogs at the computed offset
  does not apply any scaling or quality checks
  called by SetAnalog after quality verification

_SetConstantValue(unsigned short, float)
  direct shared memory write for engineering constant at slot unsigned short
  writes to /DefConstants

_SetDiscretValue(unsigned short, long)
  direct shared memory write for discrete point at slot unsigned short
  writes to /SostDiscrets

_SetImpulsValue(unsigned short, long)
  direct shared memory write for impulse counter at slot unsigned short
  writes to /SostImpulses
```

Initialization and service:

```
InitModule()
  library initialization entry point called by qmicro after dlopen
  initialises all timer slots to zero
  connects to the libservdrv service channel via LoadServDrv
  registers the calculation function table with the service registry
  sets up the shared memory connections to /SostAnalogs, /SostDiscrets,
  /SostImpulses, /DefConstants, and /DefFormula

LoadServDrv()
  registers the library with libservdrv.so.1
  called by InitModule and by the library's constructor

FuncQ global at 0x0000ceb0
  the function dispatch table registered with libservdrv
  qmicro calls library functions by index through this table rather than
  by direct function pointer, allowing the library to be replaced without
  recompiling qmicro

GetRashAnalog global at 0x0000ceb0+offset
  pointer to the RashetAnalog function registered in FuncQ

GetRashDiscret global at 0x0000ceac+offset
  pointer to the RashetDiscret function registered in FuncQ
```


## libjevent.so Event Journaling Module

The libjevent.so library maintains the operational event journal. It is
loaded by qmicro from the [Module] section of start.ini:

In the active configuration: Module=libjevent.so o/flashdisk/jevent.cfg
f/flashdisk/event1 s1000

The o parameter specifies the journal configuration file path, f specifies
the journal output file path prefix, and s1000 sets the ring buffer to 1000
event slots.

The journal records two categories of events: telestatus control events
(JournalTsCtl, 1024 slot ring buffer) and USO link events (JournalUsoLink,
1024 slot ring buffer). Each ring buffer operates independently with its own
head and tail pointers.

Configuration is loaded from jevent.cfg which contains event enable bitmasks
(MaskEvent), journal size limits (SizeJournal), and the file paths for
FileJournal (current journal) and FilePrevJournal (previous journal, retained
for audit purposes). FileConfig stores the path of the configuration file
itself.

All exports confirmed from nm:

Event recording:

```
AddTsEventJournal(unsigned char, unsigned short, unsigned char)
  records a telestatus change event
  unsigned char:  event type code (transition type: 0->1 or 1->0)
  unsigned short: telestatus point address
  unsigned char:  new state value
  called by qmicro's main cycle when a discrete point changes state
  checks MaskEvent to determine if this point is configured to generate
  journal entries before recording

AddUsoEventJournal(unsigned char, unsigned short, unsigned short,
                   unsigned char)
  records a USO device link state change event
  unsigned char:  event type (link established or link lost)
  unsigned short: USO device number
  unsigned short: device address
  unsigned char:  new link state
  called when a field device stops responding or resumes responding

AddEventProtokol(unsigned char, unsigned char, unsigned char*)
  records a protocol-level event
  unsigned char:  protocol event type
  unsigned char:  source device index
  unsigned char*: event data bytes
  used for protocol handshake events, session establishment, and error
  conditions that are not associated with a specific I/O point

AddEventDiscret(JOURNAL_CTL_TS_VALUE*, unsigned short)
  records a complete discrete event record
  JOURNAL_CTL_TS_VALUE*: pre-built event struct with timestamp and value
  unsigned short: point address
  lower-level than AddTsEventJournal, used when the event struct is
  already prepared by the caller

AddEventUsoLink(JOURNAL_CTL_TS_VALUE*, unsigned short)
  records a complete USO link event record
  same structure as AddEventDiscret but for the USO link journal

AddRecordEventProtokol(unsigned char*, int)
  adds a raw byte record to the protocol event journal
  unsigned char*: the raw record bytes
  int: record length
```

Event delivery:

```
WriteEventTs(unsigned char, EV_TIME*)
  writes a telestatus event to the journal file
  unsigned char:  event code
  EV_TIME*:       event timestamp struct
  appends the formatted event record to the current FileJournal

WriteEventUsoLink(unsigned char, unsigned short, EV_TIME*)
  writes a USO link event to the journal file
  unsigned char:  link event code
  unsigned short: device address
  EV_TIME*:       event timestamp

GoToNewJournal()
  closes the current journal file and opens a new one
  renames FileJournal to FilePrevJournal
  creates a new FileJournal file
  resets KolRecordInJournal to zero
  called when the journal file reaches SizeJournal records
```

Configuration parsing:

```
RaspakIncludeEvent(char*)
  parses an event include rule from jevent.cfg
  char*: the configuration line to parse
  adds an event type to the MaskEvent enable bitmask

RaspakExcludeEvent(char*)
  parses an event exclude rule from jevent.cfg
  removes an event type from MaskEvent

RaspakTs(char*, unsigned short*, unsigned char*)
  parses a telestatus point specification from the configuration
  char*: config line, unsigned short*: output address, unsigned char*: type

RaspakUso(char*, unsigned short*)
  parses a USO device specification from the configuration

RaspakKey(char*, char*)
  reads a key-value pair from the configuration file
  general purpose key parser used by ReadConfig

Read_Key(char*, char*)
  reads the value for a specific key from the configuration
  char*: key name, char*: output value buffer

Read_Key_Value(char*, char*)
  reads a key-value pair including the key name
  char*: input line, char*: output value buffer

ReadConfig()
  reads the full jevent.cfg configuration file
  calls RaspakIncludeEvent, RaspakExcludeEvent, RaspakTs, and RaspakUso
  for each relevant line

ReadString(int, char*, int)
  reads a single line from a file descriptor
  int: file descriptor, char*: output buffer, int: buffer size
```

Journal file management:

```
InitModule()
  initialises the library after loading by qmicro via dlopen
  reads jevent.cfg via ReadConfig
  opens or creates the journal files
  initialises the ring buffer head and tail pointers

LoadServDrv()
  registers with libservdrv.so.1

OpenNewJournal()
  opens a new journal file after GoToNewJournal
  creates the file with the current timestamp in the filename

CloseJournal()
  flushes and closes the current journal file
  called during clean shutdown

TestJournal()
  validates the current journal file for corruption
  reads the record count and verifies it against KolRecordInJournal

MakeCrc(unsigned char*, int)
  computes a CRC over a journal record for integrity checking
  unsigned char*: data buffer, int: length
  the CRC algorithm used by libjevent is distinct from the protocol CRCs
```

Text utilities:

```
alltrim(char*)
  removes leading and trailing whitespace from a string in place

ltrim(char*)
  removes leading whitespace only

trim(char*)
  removes trailing whitespace only
```

Notable globals:

```
JournalTsCtl at 0x00005040    ring buffer: 1024 JOURNAL_CTL_TS_VALUE records
JournalUsoLink at 0x00005440  ring buffer: 1024 JOURNAL_CTL_TS_VALUE records
KolJournalTsCtl at 0x00005a40 current count in JournalTsCtl ring
KolJournalUsoLink at 0x00005a41 current count in JournalUsoLink ring
KolRecordInJournal at 0x00005c24 total records written to current file
MaskEvent at 0x00004ec0        event type enable bitmask
SizeJournal at 0x00004240      maximum records per journal file
SharedObject at 0x00005c1c     libservdrv registration handle
FuncQ at 0x00005c20            function dispatch table for libservdrv
OldAddEventProtokol at 0x00005c28
  legacy event logging function pointer
  preserved for backward compatibility with older protocol software
  points to the old-format event recording code that was replaced by
  AddEventProtokol in a previous firmware version
FileConfig at 0x00004260       configuration file path
FileJournal at 0x00004680      current journal file path
FilePrevJournal at 0x00004aa0  previous journal file path (retained on rotation)
```


## libterm.so HMI Panel Driver

The libterm.so library implements the local HMI (human-machine interface)
panel driver. It is loaded from the [Module] section of start.ini only in
the project configuration (which includes Module=libterm.so) and not in
the active deployment configuration.

The panel hardware uses two QNX device files:
  /dev/term_kbd for keyboard input
  /dev/term_lcd for display output

The source file is term.cc confirmed from the strings section.

The library uses ionotify to receive keyboard events asynchronously and
MsgSendPulse/MsgReceivePulse for inter-thread communication between the Kbd
input thread and the Term display thread.

Display capabilities confirmed from format strings:
  "%.02ld.%.02ld.%.04ld %.02ld:%.02ld:%.02ld     " for date-time display
  "%.02ld.%.02ld.%.04ld~%.02ld:%.02ld:%.02ld" for compact date-time
  "%.02lu.%.02lu.%.04lu" for date display only
  "%.02lu.%.02lu.%.04lu-%.08lX " for date with hex identifier
  "%.0f " for integer-precision float
  "%.2f" for two decimal places
  "%.3f" for three decimal places
  "%ld " for signed integer
  "%lXH " for hex display with H suffix
  "%lXH/%.2f " for hex and float combined (used for raw/scaled value pairs)
  "%%.%ldf" for dynamically configured decimal precision
  " %lu " for unsigned integer with spaces

All exports confirmed from nm:

Panel initialisation:

```
InitModule()
  entry point called by qmicro after dlopen
  opens /dev/term_kbd and /dev/term_lcd
  prints "Error Init led" if LED initialisation fails
  prints "Error ReadAnsw()  (%d)" on read errors during init
  prints "Can't open panel devices!" if either device fails to open

LoadServDrv()
  registers the library with libservdrv.so.1

InitPanel(TAG_PANEL*, char const*, char const*)
  initialises a panel descriptor struct
  TAG_PANEL*: output panel descriptor
  char const*: keyboard device path
  char const*: display device path
  configures the TAG_PANEL struct with file descriptors and capabilities

InitLED(TAG_PANEL*)
  initialises the LED indicators on the panel
  writes the initial LED state pattern to the display device

SendCommandLED(int, unsigned short, unsigned short, unsigned char*)
  sends a command to the LED driver
  int: command type (on/off/blink)
  unsigned short: LED group address
  unsigned short: LED bitmask
  unsigned char*: extended command data
```

Cursor control:

```
CursorHome(TAG_PANEL*)
  moves cursor to position (0,0)

CursorOff(TAG_PANEL*)
  disables cursor display

CursorOn(TAG_PANEL*)
  enables solid cursor display

CursorUnderline(TAG_PANEL*)
  enables underline cursor display

MoveCursor(TAG_PANEL*, unsigned char, unsigned char)
  moves cursor to absolute position (row, column)
```

Display output:

```
PutString(TAG_PANEL*, char*)
  writes a null-terminated string at the current cursor position
  performs character encoding translation for Russian characters

PutNewString(TAG_PANEL*, char const*)
  writes a string with automatic line advancement
  used for scrolling display updates

UpdateMenu(unsigned short)
  redraws the current menu state on the display
  unsigned short: menu item index to highlight

UpdateItem(unsigned short)
  redraws a specific menu item
  unsigned short: item index

UpdateItemInfo(unsigned short)
  redraws the information field for an item
  unsigned short: item index

UpdateDefItem(unsigned short)
  redraws an item's configuration value
  unsigned short: item index

UpdateStartInit(unsigned short)
  redraws the start.ini display for the specified item
  unsigned short: item index

UpdateTime()
  redraws the time display in the status bar

StartMainMenu()
  renders the complete main menu on the display
  called on startup and after returning from a submenu

ShowDefItem()
  displays the detailed configuration view for the selected item
  shows all DEF_* parameters for the currently selected point

ShowItemInfo()
  displays the runtime information view for the selected item
  shows current value, quality flags, and timestamp

ShowSostItem()
  displays the status view for the selected item

ShowNoItem(unsigned short)
  displays the "no item" placeholder when the selected index is empty
```

Input handling:

```
Kbd(void*)
  keyboard input thread entry point
  runs continuously, reading keyboard events via ionotify
  translates raw key codes to application key names using KeyName table
  sends key events to the Term thread via MsgSendPulse

ReadAnsw(int, KBD_PRIEM*, unsigned char*)
  reads a keyboard response
  int: file descriptor, KBD_PRIEM*: response struct, unsigned char*: buffer

ReadData(int, unsigned char*, unsigned short)
  reads raw data from the keyboard device
  int: fd, unsigned char*: buffer, unsigned short: byte count

ReadNewString(char const*, char*)
  reads a new string value entered via the keyboard
  char const*: prompt, char*: output buffer

KeyPressed()
  checks whether a key has been pressed since the last check
  returns the key code or zero if no key is pending
```

Main display thread:

```
Term(void*)
  main display thread entry point
  receives key events from Kbd via MsgReceivePulse
  dispatches to menu navigation and display update functions
  implements the full menu navigation state machine:
    up/down keys scroll through MainMenu items
    enter key activates a submenu or enters edit mode
    escape key returns to the parent menu
  reads start.ini via the DisplayName-configured path to show
  current configuration values
```

Menu data structures (static initialised data):

```
MainMenu at 0x00008920
  the top-level menu descriptor
  contains pointers to item display functions and label strings

PodMenu0 through PodMenu4 at 0x00008934 through 0x00008994
  five submenu descriptors for the five submenu levels
  each submenu contains item count, item label array, and function pointers

KOL_POS_PODMENU at 0x00008840
  the count of valid submenu positions (5, matching PodMenu0 through PodMenu4)

KeyName at 0x00008860
  lookup table mapping raw key codes to application key name strings

TypeItem0 through TypeItem4 at 0x000088de through 0x0000890c
  type discriminators for each menu item category
  used to select the correct display format for different I/O point types

Function at 0x000088d4
  the function dispatch table for menu action handlers
  indexed by menu item type and action code
```

The KbdBuffer global at 0x000096e0 is the keyboard event ring buffer
shared between the Kbd and Term threads. The DisplayName global at 0x000088a0
holds the display identifier string used to locate the start.ini path.

The AddElements and AddSoobFlagDisable functions handle dynamic menu content:
AddElements adds a new item to the menu display buffer, and AddSoobFlagDisable
marks a specific flag as disabled in the display.

The Decoding function handles character set translation for Russian text in
the display labels, converting between KOI8-R (used in configuration files)
and the display's native encoding.


## libtmkorr.so GPS Time Correction Library

The libtmkorr.so library manages GPS-based time correction. It connects to
the gpstime daemon via the //gpstime shared memory segment and provides time
correction data to the channel drivers.

Exports confirmed from earlier analysis:
  ConnectToGps: connects to the gpstime shared memory segment
  IsTimeKorrectEnable: checks whether GPS-based correction is currently valid
  InitModule: library initialization entry point

The InitModule function performs a channel code calculation based on an input
word at offset 0x2a in the library's configuration, which is used to select
the correct GPS receiver protocol variant.


## libsystypes.so.1 Shared Type Definitions

The libsystypes.so.1 library provides shared type definitions and common
utility functions used across the firmware. Key exports include:

```
_MakeKvitok: the actual kvitok frame construction function
  signature: uint16, SOST_PRIEM*, SOST_SEND*, uint8*, uint8*, uint16,
             RTOS_RETRANSLATE_ADR*, uint16
  this is the implementation called by the kanaldrv vtable method SendKvitok

_PriemPacket: the frame accumulation state machine
  called via vtable slot at obj+0x128 in kanaldrv
  implements the SOST_PRIEM FSM for byte-by-byte frame reception

_AnalizBufPriem: the incoming frame analyzer
  called via vtable slot at obj+0x15c in kanaldrv

_AnalizPriem: simplified frame analysis for the init phase

_MakeNextPachka: builds the next packet in a batch transmission sequence
  used by the SERPR retranslation path

_PrepareBufRetr: prepares the retranslation buffer

ConvertTimeToSystem(TIME_SERVER*, SYSTEMTIME*)
  converts a GPS TIME_SERVER struct to Windows-compatible SYSTEMTIME

ConvertTimeKanalToSystem(TIME_SERVER_KANAL*, SYSTEMTIME*)
  converts a 6-byte BCD timestamp to SYSTEMTIME

Get_Local_Time: time retrieval function

CopyIdent(IDENT_PAR_KANAL*, IDENT*)
  copies an IDENT struct into an IDENT_PAR_KANAL, filling extended fields

_MakeTu(IDENT_PAR_KANAL*, uint8_t, uint8_t)
  builds a telecontrol command from an IDENT_PAR_KANAL and two byte parameters

_AddNregBuf, _AddPerBuf, _DeleteEvent, _DeletePerEvent
  ring buffer management functions for the event and periodic data queues
```


## libwatch86dx.so and libwatch586.so Hardware Watchdog Drivers

Both watchdog libraries implement the hardware watchdog for the PC/104 board.
libwatch86dx.so targets the x86 DX variant board and libwatch586.so targets
586-class hardware.

Both export WatchDog(int) and startcikl(void*). The WatchDog function sends
a periodic pulse to the hardware watchdog circuit to prevent system reset.
The startcikl function is the watchdog thread entry point. The internal tick
function pointer is stored in the global ____Cikl (four leading underscores,
an unusual naming convention suggesting it is a compiler-generated or
linker-generated symbol name).

The watchdog driver is loaded from the [Module] section of start.ini:
Module=libwatch86dx.so (in the active configuration) or the 586 variant for
alternative hardware.
