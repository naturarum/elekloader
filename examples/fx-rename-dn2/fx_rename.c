/* SPDX-License-Identifier: GPL-2.0-or-later */
/* fx-rename (Digitone II OS 1.12): names on the screen for dn2-cfw's FTN1 build, where the
 * chorus slot runs our flanger and the delay slot runs our tape delay (both on the DSP).
 * This mod renames labels only: no new rows, knobs, glyphs or code. Every knob keeps its
 * parameter row, MIDI CC, storage and frame word, and the knob labels on both pages stay
 * (DPTH SPD HPF WDTH; TIME X WID FDBK HPF LPF). Emulator-tested only (dn2-cfw
 * (github.com/naturarum/dn2-cfw) notes/T5.0b-rename.md).
 *
 * Everything is in mod.json's sites, written by dn2-cfw's scripts/t50b_make_mod.py, which
 * checks each stock pointer and instruction against the stock image first:
 *
 * - `ptr` sites on the parameter descriptor table (0x401f83a4, rows of 0x3c bytes; +0x28 the
 *   long name shown as "Name=value" in the header, +0x30 the short label under the knob).
 *   The table is in the image and is read each time a page is drawn, so pointing a row at a
 *   string below takes effect from the first frame. The stock strings stay as they are: "DEL",
 *   "CHR" and "Delay Send" are shared with rows this mod does not rename (the AMP and filter
 *   envelopes' DEL, for one).
 * - `keep2` sites on three `pea` instructions per effect: the two that build the page record's
 *   names at boot (static initializer 0x400c2c4e: the short name and the title, "Delay (1/3)")
 *   and the one that draws the box at the left of the SEND FX page (0x4006f4a8). The page
 *   record copies the string into a std::string when the static initializer runs, which is
 *   after core's boot copy has put these strings at 0x47E00000 (the OS entry's call at
 *   0x40000538 comes first).
 * - one `ptr` site on the modulation destination lists' group names (0x401f7b04, by group - 5):
 *   group 0x12, the delay's parameters, "DEL" in stock.
 *
 * Widths (fonts read from the image): the 4x5 knob-label font draws "TAPE" 18 px wide, as wide
 * as "DPTH" (FDBK and WDTH are 19), so the delay sends become TAPE. The SEND FX page box is
 * 13 px inside its frame and its 3x6 font draws "TAPE" 15 px wide, so the box says TAP. The
 * external-input L/R sends become LTAP/RTAP (17/18 px), like stock's LDEL/RDEL.
 */

/* the chorus slot: our flanger */
const char fxr_flanger[] = "Flanger";             /* SEND FX 3 title: "Flanger (3/3)" */
const char fxr_flg[] = "FLG";                     /* page box, short name, sends' label */
const char fxr_flg_send[] = "Flanger Send";       /* track FX G */
const char fxr_flg_mix[] = "Flanger Mix Vol.";    /* MIXER, FX MIX page */
const char fxr_flg_in[] = "In Flanger Send";      /* external input sends */
const char fxr_flg_in_l[] = "In Flanger Send L";
const char fxr_flg_in_r[] = "In Flanger Send R";
const char fxr_flg_l[] = "LFLG";
const char fxr_flg_r[] = "RFLG";

/* the delay slot: our tape delay */
const char fxr_tape[] = "Tape";                   /* SEND FX 1 title: "Tape (1/3)" */
const char fxr_tap[] = "TAP";                     /* page box and short name: TAPE does not fit */
const char fxr_tape_s[] = "TAPE";                 /* sends' label, destination group */
const char fxr_tape_send[] = "Tape Send";         /* track FX E, flanger page G */
const char fxr_tape_mix[] = "Tape Mix Vol.";      /* MIXER, FX MIX page */
const char fxr_tape_in[] = "In Tape Send";        /* external input sends */
const char fxr_tape_in_l[] = "In Tape Send L";
const char fxr_tape_in_r[] = "In Tape Send R";
const char fxr_tape_l[] = "LTAP";
const char fxr_tape_r[] = "RTAP";
