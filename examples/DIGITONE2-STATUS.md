# Digitone II 1.12 examples: where each one has run

The headers and `mod.json` descriptions of these examples were written before they ran on a unit, and are left
unchanged so the builds stay byte-for-byte reproducible. This file holds the current status. Builds and evidence:
[dn2-cfw](https://github.com/naturarum/dn2-cfw) (`ROADMAP.md`, `HANDOFF.md`, `notes/`).

| example | status |
|---|---|
| `mods/core-dn2` | **ran on a unit** (2026-10-13): in DH01, FTN1, GT00, GT01 |
| `midi-hooktest-dn2` | **ran on a unit** (DH01, 2026-10-13); its MIDI-out effect not yet watched there |
| `fx-rename-dn2` | **ran on a unit** (FTN1, 2026-10-13): every name displays correctly |
| `param-grow-dn2` | **ran on a unit** (GT00, 2026-10-13): normal use, +Drive save and load, cold start |
| `param-rows-dn2` | **ran on a unit** (GT01, 2026-10-13) |
| `fx-echo-dn2` | emulator only so far (SE01, brick checker 12/12, 2026-10-13); AGE's label does not show its value while turning (known, minor) |
| `param-test-dn2` | emulator only. **Never flash it** |
| `fx-page2-dn2` | emulator only. **Never flash it**: it borrows rows that carry MIDI CCs, and its SHPE row copies a lockable display entry, so a p-lock on it would be saved as a lock on the track's sound slot 70 (dn2-cfw `notes/T6.1-table-growth-study.md`) |
| `flanger-ui-dn2` | emulator only, superseded by `fx-rename-dn2` |

New rows added through `param-grow` must keep their storage slot k at 100 or below: param-grow raises the map
builder's bound, and the builder files rows by k into tables of 101 entries. dn2_firmware_explore found the same
limit on 1.11 for their lfo4 (which grew this table first, on 1.11) and keeps that bound at 321 instead.
