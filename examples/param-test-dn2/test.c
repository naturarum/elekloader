/* SPDX-License-Identifier: GPL-2.0-or-later */
/* param-test (Digitone II OS 1.12): two new parameter rows, TEST and TST2, on the flanger
 * (chorus) page's empty knobs E and F, through param-grow's table param_rows. FOR THE EMULATOR
 * ONLY: never flash it (dn2-cfw (github.com/naturarum/dn2-cfw) notes/T6.5-table-growth.md,
 * build GT1T).
 *
 * What it tests: a row past the stock ones (the last one, 0x17f, so that anything sized
 * for fewer rows would be overrun) that is drawn, turned and named by the stock page code,
 * stored in the kit by the stock code, and reached by MIDI CC and NRPN through the resized
 * input cache. To be stored without any new storage code it shares k 48 with the hidden
 * stock row 0x83, "Reverb FX Routing" (stored id 24; on no page; no CC; the stock DSP
 * stores the word and never reads it), and keeps that row's default, 1, so a new kit is
 * unchanged. Sharing a k breaks rule 3 of the study (one row per k in a set): the FX map
 * built at boot names TEST for k 48 instead of row 0x83. Harmless for a test, and the reason
 * this mod is not for a unit.
 *
 * CC 3 and NRPN 307 are free on the FX channel in stock (CC 3 is not one the CC parser
 * takes for itself). Display: the chorus DPTH knob's entry (flags 1) with bit 3 set: never
 * p-lockable. No randomiser. Overbridge id 0x43e (0x400 + 0x3e).
 *
 * TST2 (row 0x17e) is the other kind: an FX slot no stock row uses, k 87 (the compressor rows
 * of param-rows take 70..86). The stock storage keeps only k 25..69, so it shows what a kit
 * save and reload do to such a value, and that the frame to the DSP does not carry it.
 * Default 0, no CC, no NRPN, never p-lockable, Overbridge id 0x43d.
 */
#include "../param-grow-dn2/param_rows.h"

const struct param_row test_rows[] __attribute__((aligned(4))) = {
    { 0x17f, { 0x10, 48, V(0), V(127), V(1), 0, 3, -1, 307, 0x400 + 0x3e, 0,
               "Test", STR_CHORUS, "TEST", FMT_NUMBER, STR_EMPTY },
      0x69, 8, 0, 19, 4 },
    { 0x17e, { 0x10, 87, V(0), V(127), V(0), 0, -1, -1, -1, 0x400 + 0x3d, 0,
               "Test 2", STR_CHORUS, "TST2", FMT_NUMBER, STR_EMPTY },
      0x69, 8, 0, 19, 5 },
    { 0 }
};
