# Digitone II 1.12 examples: where each one has run

The headers and `mod.json` descriptions of these examples were written before they ran on a unit, and are left
unchanged so the builds stay byte-for-byte reproducible. This file holds the current status. Builds and evidence:
[dn2-cfw](https://github.com/naturarum/dn2-cfw) (`ROADMAP.md`, `HANDOFF.md`, `notes/`).

| example | status |
|---|---|
| `mods/core-dn2` | 0.1 **ran on a unit** (2026-10-13): in DH01, FTN1, GT00, GT01. 0.2 (page records, `core_pagerecs`): emulator only so far (CMP1, 2026-10-13) |
| `midi-hooktest-dn2` | **ran on a unit** (DH01, 2026-10-13); its MIDI-out effect not yet watched there |
| `fx-rename-dn2` | **ran on a unit** (FTN1, 2026-10-13): every name displays correctly |
| `param-grow-dn2` | **ran on a unit** (GT00, 2026-10-13): normal use, +Drive save and load, cold start |
| `param-rows-dn2` | 0.1 **ran on a unit** (GT01, 2026-10-13). 0.2 (the band rows' names, ranges, defaults and texts): emulator only so far (CMP1) |
| `comp-ui-dn2` | emulator only so far (CMP1, 2026-10-13): the three band pages in the MIXER view; needs core 0.2 |
| `fx-echo-dn2` | emulator only so far (SE01, brick checker 12/12, 2026-10-13); AGE's label does not show its value while turning (known, minor) |
| `param-test-dn2` | emulator only. **Never flash it** |
| `fx-page2-dn2` | emulator only. It patches the page-record routine 0x400c25d8 itself, so it links only with core 0.1 (0.2 owns that routine). **Never flash it**: it borrows rows that carry MIDI CCs, and its SHPE row copies a lockable display entry, so a p-lock on it would be saved as a lock on the track's sound slot 70 (dn2-cfw `notes/T6.1-table-growth-study.md`) |
| `flanger-ui-dn2` | emulator only, superseded by `fx-rename-dn2` |

dn2-cfw's builds made before core 0.2 and param-rows 0.2 (DC01, DH01, FP01, FTN1, GT00, GT01, SE01) build those
two mods from this fork's commit 59cd41f (their 0.1 sources), so they still rebuild byte for byte: built from the
working tree, DC01 and DH01 (DH01 ran on a unit) give other containers, and FP01 is refused (core 0.2 and fx-page2
both patch 0x400c25d8). dn2-cfw's `scripts/elk_pin.py` extracts that commit's sources.

New rows added through `param-grow` must keep their storage slot k at 100 or below: param-grow raises the map
builder's bound, and the builder files rows by k into tables of 101 entries. dn2_firmware_explore found the same
limit on 1.11 for their lfo4 (which grew this table first, on 1.11) and keeps that bound at 321 instead.
