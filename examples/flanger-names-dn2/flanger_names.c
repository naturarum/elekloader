/* SPDX-License-Identifier: GPL-2.0-or-later */
/* flanger-names (Digitone II OS 1.12): the flanger's names on the screen for dn2-cfw's FL01 build, where the chorus
 * slot runs our flanger (its knob C is TIME, the centre delay) and the delay slot is Elektron's own delay
 * (github.com/naturarum/dn2-cfw, notes/T5.6-flanger-retune.md, notes/T6.3-cmp4.md). fx-rename-dn2's 13 flanger sites
 * with the same strings, none of its 16 tape sites (every delay name stays stock), and since 0.2 the chorus page's knob
 * C: Time/TIME in place of High-pass/HPF, and a new-kit default of 32 (the flanger of CMP4). Labels and one default
 * only: no new rows, knobs, glyphs or code; every knob keeps its parameter row, MIDI CC, storage and frame word.
 *
 * Everything is in mod.json's sites, written by dn2-cfw's scripts/fl01_make_mod.py (0.2) from
 * scripts/cmp4_make_mod.py's 0.1 sites, which check each stock pointer and instruction against the stock image first
 * (as fx-rename's scripts/t50b_make_mod.py; the reasons for each site are in dn2-cfw notes/T5.0b-rename.md): `ptr`
 * sites on six rows of the parameter descriptor table (+0x28 the long name, +0x30 the short label), one `bytes` site
 * on row 0x6b's default (+0x10), and `keep2` sites on the three `pea` instructions that name the chorus page (its
 * title and short name, built at boot) and the SEND FX page's box.
 */

const char fln_flanger[] = "Flanger";             /* SEND FX 3 title: "Flanger (3/3)" */
const char fln_flg[] = "FLG";                     /* page box, short name, sends' label */
const char fln_flg_send[] = "Flanger Send";       /* track FX G */
const char fln_flg_mix[] = "Flanger Mix Vol.";    /* MIXER, FX MIX page */
const char fln_flg_in[] = "In Flanger Send";      /* external input sends */
const char fln_flg_in_l[] = "In Flanger Send L";
const char fln_flg_in_r[] = "In Flanger Send R";
const char fln_flg_l[] = "LFLG";
const char fln_flg_r[] = "RFLG";
const char fln_time[] = "TIME";                   /* 0.2: the chorus page's knob C label (was HPF) */
const char fln_time_name[] = "Time";              /* 0.2: its long name, "Time=32" in the header (was High-pass) */
