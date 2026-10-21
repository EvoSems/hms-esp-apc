# Firmware work log — APC SRT2200XLI

This log records only source/test evidence available in this project handoff. It does **not** assert that a particular binary has been flashed to the ESP32.

## v1.16.9 descriptor-capacity follow-up — 2026-10-19

Inspection of the exact v1.16.8 ZIP used by the user showed why the claimed vendor Feature polling was incomplete: `HID_PDC_MAX_FIELDS` was 224, and the SRT's long vendor reports consume dozens of entries each. In addition, later vendor Feature fields use Constant flags and were filtered as padding. The parser now holds 512 fields and retains explicitly named vendor-page usages. The regression fixture exceeds 224 fields and verifies report IDs `0x8D`, `0x8E`, `0x92`, `0x93`, and `0x94` survive and enter the Feature poll list.

The actual live capture previously showed only `0x96 FEAT` with a two-byte response (`96 00`). That is a short response relative to the declared 63-byte payload; the firmware should record it as returned, not infer payload semantics. The `0x89` report is an Input report, so Feature polling does not request it; it can only be decoded if received from the interrupt endpoint or separately queried with an Input GET_REPORT implementation. The current fix focuses on making all declared Feature IDs available for evidence gathering. Input voltage, load percentage and actual output power remain unresolved.

Native verification on this source: `./test/run-tests.sh` passed (107 HID walker checks; 201 truncated inputs and 3,200 mutations survived; 35 legacy report decoder checks; 16 SRT descriptor regression checks). No ESP-IDF build or physical-device flash was performed in this environment.

## Confirmed from the latest live diagnostic supplied on 2026-10-19

- UPS identifies as American Power Conversion / Smart-UPS SRT, serial `AS2036192853`, UPS firmware `15.5`, chemistry `PbAc`.
- The prior shadow-decode comparison is historical only. This follow-up showed that matching zeroes did not establish correct mappings, and several hand-written report IDs were misinterpreted. See the sensor mapping corrections below for the current interpretation and remaining limits.
- The latest live `/hid` values supplied on 2026-10-19 show charge `94%`, runtime `40703 s` (~11 h 18 min), battery voltage `82.31 V`, nominal battery voltage `72.00 V`, online `1`, charging `1`, discharging `0`, low battery `0`, overload `0`, replace battery `0`.
- The latest raw samples are `0C 5E 00` (94%), `0D FF 9E 00 00` (40703 seconds), `0B 27 20` (82.31 V at 0.01 V scale), and `0A 20 1C` (72.00 V at 0.01 V scale). Regression tests now include the latest charge/runtime samples as well as prior samples.
- `input_voltage`, `load_percent`, and `nominal_power` still read `0` in both decoder columns. Agreement at zero is not proof those values are correctly supported or mapped; leave these as unresolved until the descriptor/report captures show the relevant usage and live report values.
- The source's local change corrects RemainingCapacity usage page to `0x84` and the matching usability check; the test descriptor was updated to match.
- The hand-written parser no longer interprets report `0x0D` as battery voltage or report `0x13` as AC-present; it also decodes report `0x07` as the documented status bitfield. These changes are present in the supplied source diff; this work log does not independently verify the hardware semantics beyond the latest diagnostic.

## Host-side verification

Run from the project root:

```sh
./test/run-tests.sh
```

This compiles the HID descriptor walker and mapping test natively, without ESP-IDF or hardware. The test run during packaging completed with **107 checks, 0 failures**, and the parser survived **201 truncated inputs plus 3,200 mutations**.

## Not verified / still outstanding

- A clean ESP-IDF firmware build from this exact source tree. The user's earlier Mac build attempt was blocked by a stale CMake build directory pointing to a different path. Generated build directories have therefore been excluded from this source ZIP so the next build can configure cleanly.
- Flashing this source revision to the device, and confirming the running firmware revision.
- MQTT discovery/state values and Home Assistant entity behaviour after flashing.
- The correct live-report mappings for input voltage, load percentage, and nominal power.

## Packaging note

Generated `build/` and `build-n8r8/` directories and local backup copies of source files are intentionally excluded from this clean source package. They can contain stale absolute paths and are not source-of-truth files. The current tracked and uncommitted source edits from the supplied archive are retained.

## Sensor mapping corrections — 2026-10-19 follow-up

Applied source fixes based on the user's latest 449-byte SRT2200XLI HID descriptor and raw report samples:

- Report `0x0D` is now decoded as the 32-bit little-endian `RunTimeToEmpty` in seconds. Sample `0D 35 70 00 00` is `28,725 s`; `0xFFFFFFFF` is treated as unavailable.
- Report `0x0B` is decoded as battery voltage using the sample's centivolt representation: `0B 27 20` -> `82.31 V`.
- Report `0x09` is decoded as the descriptor-declared packed battery manufacture date; packed value `0x5123` yields `2020/09/03`. Report `0x0A` is decoded as nominal battery voltage with centivolt scaling; a `7200` sample yields `72 V`.
- Report `0x08` is decoded as `RemainingTimeLimit`, so `08 3C 00` -> `60 s`; it is no longer mislabelled as nominal battery voltage.
- Report `0x14` is decoded as `AudibleAlarmControl`; value `1` maps to `disabled`. It is no longer treated as low-battery/shutdown-imminent flags.
- The default hard-coded `Power Failure = OK` was removed. This status is now populated from the live online flag (`OK` when online, `On battery` when offline); it is not a historical transfer-reason sensor.
- The hand-written parser now rejects report IDs outside the standard `0x07..0x14` range declared by this model’s supplied descriptor, preventing legacy mappings from unrelated APC models from overwriting readings if a fallback poll occurs.
- MQTT now supports clearing retained state. At startup, retained states for sensors not exposed by the supplied descriptor are cleared so HA does not show stale values or fake zeroes. In particular, this descriptor contains no standard Input/Output collection, PercentLoad, ConfigActivePower, transfer point, sensitivity, historical line-fail cause, or self-test-result field. Those sensors remain unknown until a real supported report mapping is established; no unsupported mapping was invented.
- Input voltage and load are no longer published as `0` simply because the parser struct starts at zero. Runtime and battery voltage are only published when a positive reading has arrived. Load's HA discovery no longer uses `power_factor` device class with a percent unit.
- Firmware version constant bumped from `1.16.3` to `1.16.4`.

### Verification after these changes

- `./test/run-tests.sh`: HID descriptor walker/mapping test **107 checks, 0 failures**, including robustness against **201 truncations and 3,200 mutations**.
- Added host-side report decoding tests: **23 checks, 0 failures**, including charge `66%`, runtime `28,725 s`, battery voltage `82.31 V`, nominal voltage `72 V`, low-runtime threshold `60 s`, warning threshold `50%`, low-charge threshold `10%`, beeper disabled, and `OL CHRG` status flags.
- Parser source passes host compiler syntax checking with small ESP-IDF/FreeRTOS logging stubs.
- A full ESP-IDF firmware build, flashing, and post-flash MQTT/Home Assistant validation have **not** been run in this environment. Those remain required before calling the firmware/hardware result fully verified.


## 2026-10-19 — SRT2200XLI descriptor regression fix (v1.16.5)

### Confirmed root cause in source

The current `hid_pdc.c` skipped every main item with the HID Constant flag. In
this UPS's descriptor, actual mapped Power Device/Battery usages are tagged with
`0x23`/`0xA3` and nevertheless return live values. Consequently the generic map
lost the very fields needed to derive Feature polls for `0x0A`, `0x0B`, `0x0C`,
`0x0D` and `0x07`; it was expected to stay at zero. The hand-written parser
could only see these values if the report IDs were polled, but the fallback list
was for a different APC family and omitted the actual voltage IDs.

### Changes made

- Preserve constant-flag fields only when they have explicit Power Device or
  Battery usages; anonymous constant padding is still skipped and continues to
  advance the report bit cursor.
- Replace the stale fallback report list with the SRT2200XLI's actual supported
  IDs (`0x07`–`0x14`, excluding unsupported/unmapped `0x0E`/`0x10`).
- Decode `0x12` as `DelayBeforeShutdown` and `0x13` as `DelayBeforeReboot`, with
  32-bit length checks and all-ones sentinel rejection. Descriptor-driven numeric
  fields also ignore all-ones sentinels and preserve the last valid sample.
- Publish shutdown delay only when a valid positive reading exists; stop
  publishing the legacy `shutdown_timer` struct default as zero.
- Bump firmware version to `1.16.5`; add `test/apc_srt2200xli_descriptor_test.c`.

### Verification / limits

- `./test/run-tests.sh`: **107 + 31 + 10 checks, 0 failures**.
- The new descriptor regression test confirms `0x0A`/`0x0B` are included in the
  derived Feature poll list and validates charge, runtime, voltage scaling and
  status decoding against the supplied sample payloads.
- This environment does not have ESP-IDF (`idf.py`) installed, so no firmware
  image was built here. The code is not yet verified on the physical UPS after
  flashing. The supplied descriptor does not declare an `Input` collection or
  `Input.Voltage`; the firmware must leave measured mains input voltage unknown
  rather than infer it from battery voltage or publish a fake zero.


## Live capture follow-up — v1.16.6

- New live `/hid` evidence: hand-written charge = 93%; descriptor shadow = 0%; raw interrupt report `0C 5D 00`; descriptor table identifies report `0x0C` Input as `UPS.PowerSummary.RemainingCapacity`.
- Added a narrowly scoped APC SRT (VID:PID `051D:0003`) compatibility guard in the shadow decoder. For Input report `0x0C`, it accepts only values 0–100 and restores the value if the generic mapper did not apply it or disagreed. A warning log reports the mapped value, raw percentage and applied-field count to help diagnose why the generic path diverged.
- Added test coverage for `0C 5D 00` -> 93%. Existing behavior for measured/nominal battery voltage, runtime and status remains unchanged.
- Input voltage/load/nominal power remain unpopulated: the provided standard descriptor has no `Input` collection or `PercentLoad`/`ConfigActivePower` fields. Vendor report `0x89` is opaque and was not mapped without payload evidence.
- Native tests were run after the edit. ESP-IDF build and physical flash are not available in this environment.

## Source/package audit — 2026-10-19

- The previously supplied directories labelled `hms-esp-apc-fixed-1.16.5` and `hms-esp-apc-fixed-1.16.6` contain identical source trees, and both `main/version.h` files declare firmware version `1.16.6`. The earlier folder/ZIP labels were therefore misleading; this audit package is named for the work performed rather than claiming a new firmware version.
- No speculative mappings were added for input voltage, load percentage, or nominal power. The captured standard descriptor does not expose those usages. The vendor reports (including input report `0x89`) remain opaque until their actual payloads can be correlated with UPS state changes.
- Native tests do not establish a successful ESP-IDF firmware build or a successful flash. Those checks remain outstanding.


## 2026-10-19 — v1.16.7 model identity follow-up

- Fixed the HA discovery fallback model from the unrelated `Back-UPS XS 1000M`
  to `Smart-UPS SRT2200XLI`.
- Added a descriptor-signature-gated identity correction for the case where the
  SRT2200XLI presents the stale Back-UPS product string. The override requires
  APC `051D:0003` plus report/usage pairs for remaining capacity, runtime,
  nominal voltage, battery voltage, and beeper control.
- Initial discovery now gives USB identity enumeration up to 15 seconds and allows
  a short settling interval before publishing retained Home Assistant discovery.
- Confirmed the native test suite passes on this source tree (107 descriptor
  checks, 35 report-decoder checks, 13 SRT2200XLI regression checks).
- The model-specific nominal input voltage (230 V) and rated output power
  (1980 W) are populated as static specifications, based on Schneider Electric's
  product page. They are not presented as live measurements.
- **Still unresolved:** measured mains input voltage, load percentage, and actual
  output power cannot be truthfully decoded from the supplied standard descriptor.
  The vendor-specific reports require live payload capture under known conditions.
- **Not verified:** ESP-IDF firmware build, flash, and Home Assistant registry update
  on the physical device.
