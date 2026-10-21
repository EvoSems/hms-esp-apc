# v1.16.12 delivery notes — 2026-10-19

## Recovery from v1.16.11 diagnostic regression
- v1.16.11 queried every descriptor-declared Input report. Unsupported requests could time out and the old timeout path freed a transfer without confirmed cancellation/completion, risking disruption to subsequent USB communication.
- v1.16.12 removes the broad Input sweep and probes only the previously observed vendor Input report 0x89, once per boot, and only for APC VID:PID 051D:0003.
- Normal interrupt endpoint reads and descriptor-derived Feature polling remain enabled. No Output reports are sent.
- This change is intended to restore the previous capture path; it still needs an ESP-IDF build and a live hardware check.

## Verification
- Native host-side regression tests run from this source tree.
- ZIP integrity checked after packaging.
- No ESP-IDF build or physical hardware flash performed here.

# v1.16.11 delivery notes — 2026-10-19

## Diagnostic capture expansion
- `/hid` retains interrupt reports and Feature responses and now also captures successful `GET_REPORT(Input)` responses for every Input report ID found in the parsed descriptor.
- Request failures and returned lengths are logged. This is a raw diagnostic sweep; it does not infer proprietary field meanings.
- The UPS descriptor's Output report IDs are documented but are not transmitted or probed.
- Increased raw report storage to 128 source/report slots.

## Verification
- Native test suite run from the source tree and again after extracting the final ZIP.
- ZIP integrity checked with `unzip -t`.
- No ESP-IDF build or hardware flash was performed in this environment.

## Known unresolved readings
Live input voltage, load percentage, and actual output power remain unresolved. A short `GET_REPORT(Input)` response such as `89 00` is recorded as returned; it is not treated as a valid full report or measurement.

---

# v1.16.9 delivery notes — 2026-10-19

## Included
- Complete firmware source project, web UI/flasher assets, scripts, workflows, docs, and host-side regression tests.
- Existing SRT2200XLI model identity correction for Home Assistant.
- Existing static model specifications: nominal input voltage 230 V and rated output power 1980 W. These are specifications, not live measurements.
- Increased HID descriptor field capacity from 224 to 512 so long vendor reports do not hide later report IDs.
- Constant vendor-page fields with explicit usages are retained for diagnostic report discovery; anonymous constant padding remains filtered.
- SRT regression fixture now exceeds the old field limit and verifies that vendor Feature IDs 0x96, 0x8D, 0x8E, 0x92, 0x93, and 0x94 are included in descriptor-derived polling.

## Native tests run against this source tree
- HID descriptor parser/walker: 107 checks passed; 201 truncated descriptors and 3,200 mutated inputs survived.
- APC report decoder: 35 checks passed.
- SRT2200XLI descriptor regressions: 16 checks passed, including >224 field coverage and later vendor Feature IDs.
- ZIP integrity checked; tests rerun after extracting the final ZIP.

## Not verified
- `idf.py` is not installed in this packaging environment, so no ESP-IDF firmware build was performed here.
- Firmware has not been flashed to the user's physical UPS from this package.
- Home Assistant device identity has not been confirmed after flashing this revision.

## Still unresolved live measurements
The standard descriptor does not expose standard measured input voltage, load percentage, or actual output-power usages. This version fixes truncation/filtering that hid later vendor Feature IDs and lets the firmware poll those reports. The vendor payloads' meaning is still unknown; no guessed live measurement mappings have been added. Once flashed, capture `/hid` after a polling cycle and correlate payload changes with a known load change and, if safe, a known UPS state change.


## v1.16.10 follow-up diagnostic

The live v1.16.9 capture now shows vendor Feature IDs 0x8D/0x8E/0x92/0x93/0x94, but does not show vendor Input report 0x89. This release probes report 0x89 with HID GET_REPORT(Input) and labels any response as `GETIN` in `/hid`. It does not infer voltage/load/power from the vendor Feature bytes.
