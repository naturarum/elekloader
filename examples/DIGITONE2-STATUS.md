# Digitone II 1.12 examples: where each one has run

The headers and `mod.json` descriptions of these examples were written before they ran on a unit, and are left
unchanged so the builds stay byte-for-byte reproducible. This file holds the current status. Builds and evidence:
[dn2-cfw](https://github.com/naturarum/dn2-cfw) (`ROADMAP.md`, `HANDOFF.md`, `notes/`).

| example | status |
|---|---|
| `mods/core-dn2` | 0.1 **ran on a unit** (2026-10-13): in DH01, FTN1, GT00, GT01. 0.2 (page records, `core_pagerecs`) **ran on a unit** (CMP1, 2026-10-14) |
| `midi-hooktest-dn2` | **ran on a unit** (DH01, 2026-10-13); its MIDI-out effect not yet watched there |
| `fx-rename-dn2` | **ran on a unit** (FTN1, 2026-10-13): every name displays correctly |
| `param-grow-dn2` | **ran on a unit** (GT00, 2026-10-13): normal use, +Drive save and load, cold start |
| `param-rows-dn2` | 0.1 **ran on a unit** (GT01, 2026-10-13). 0.2 (the band rows' names, ranges, defaults and texts): **ran on a unit** (CMP1, 2026-10-14). 0.3 (three band mix rows, k 87..89): **ran on a unit** (dn2-cfw CMP3, 2026-10-21) |
| `comp-ui-dn2` | 0.1 **ran on a unit** (CMP1, 2026-10-14): the three band pages in the MIXER view; needs core 0.2. 0.2 (each band page's H is the band's own mix; needs param-rows 0.3): **ran on a unit** (dn2-cfw CMP3, 2026-10-21). The bar's draw is unchanged (h = q / 655 px); CMP3's DSP sends 1310 x the band's held gain reduction in dB in half-words 8..10 (no longer Q15 of 1 - g), so the bar is 2 px per dB, 12 dB at the bottom of the CMP box |
| `comp-link-dn2` | 0.1 **ran on a unit** (CMP2, 2026-10-18): the band knobs k 70..86 reach the DSP in the SPI frame tail (one site, 0x40025e9e); needs param-rows 0.2. 0.2 (the band values and mixes saved per kit: the FX record serializer and loader and FxSetup's slot 17 hooked; old kits seeded from their stock compressor values; needs param-rows 0.3): **ran on a unit** (dn2-cfw CMP3, 2026-10-21: band settings come back after kit switches and reloads; storage review notes/T6.3-cmp3-review.md lists two minor fixes for 0.3). 0.3 (those two fixes in compl_update: the live words read before the stock routine and the words written only if the setup still points at the same live words and mirror record; the id-0 word put back whenever no k in 0..24 is named): **ran on a unit** (dn2-cfw SC01 2026-10-21 and CMP4 2026-10-22; CMP4: "looking very stable") |
| `flanger-names-dn2` | 0.1 **ran on a unit** (dn2-cfw CMP4, 2026-10-22, with core 0.2, comp-ui 0.2, param-grow 0.1, param-rows 0.3, comp-link 0.3): fx-rename-dn2 0.1's 13 flanger sites with the same strings (those ran on a unit in FTN1 to CMP3) and none of its tape sites, for a DSP that runs the stock delay again; conflicts with fx-rename |
| `fx-echo-dn2` | 0.1 emulator only (SE01, brick checker 12/12, 2026-10-13; never flashed); AGE's label does not show its value while turning. 0.2 (AGE shows its value while turning: fxe_tick clears bit 2 of row 0x7a's display flags, bit 3, never p-lockable, kept): **ran on a unit** in dn2-cfw SC01 (2026-10-21, with core 0.2, param-rows 0.3, comp-ui 0.2, comp-link 0.3); SC01's DSP stopped on echo knob turns, traced to where SC01 placed the echo's DSP code, not to this mod (dn2-cfw notes/SC01-soak.md); the Space Echo is parked |
| `param-test-dn2` | emulator only. **Never flash it** |
| `fx-page2-dn2` | emulator only. It patches the page-record routine 0x400c25d8 itself, so it links only with core 0.1 (0.2 owns that routine). **Never flash it**: it borrows rows that carry MIDI CCs, and its SHPE row copies a lockable display entry, so a p-lock on it would be saved as a lock on the track's sound slot 70 (dn2-cfw `notes/T6.1-table-growth-study.md`) |
| `flanger-ui-dn2` | emulator only, superseded by `fx-rename-dn2` |

dn2-cfw's CMP1 and CMP2 build every mod from this fork's commit e10bb05 (comp-ui 0.1, param-rows 0.2, comp-link 0.1),
so they still rebuild byte for byte after CMP3 moved those three on in place. CMP3 builds every mod from commit 4b1384b
(comp-link 0.2), and SE01 its fx-echo from 59cd41f (0.1), since SC01 moved comp-link to 0.3 and fx-echo to 0.2 in place.

dn2-cfw's builds made before core 0.2 and param-rows 0.2 (DC01, DH01, FP01, FTN1, GT00, GT01, SE01) build those
two mods from this fork's commit 59cd41f (their 0.1 sources), so they still rebuild byte for byte: built from the
working tree, DC01 and DH01 (DH01 ran on a unit) give other containers, and FP01 is refused (core 0.2 and fx-page2
both patch 0x400c25d8). dn2-cfw's `scripts/elk_pin.py` extracts that commit's sources.

New rows added through `param-grow` must keep their storage slot k at 100 or below: param-grow raises the map
builder's bound, and the builder files rows by k into tables of 101 entries. dn2_firmware_explore found the same
limit on 1.11 for their lfo4 (which grew this table first, on 1.11) and keeps that bound at 321 instead.
