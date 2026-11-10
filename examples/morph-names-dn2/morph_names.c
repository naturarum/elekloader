/* SPDX-License-Identifier: GPL-2.0-or-later */
/* morph-names (Digitone II OS 1.12): the reverb page for MRB, the morphing reverb of dn2-cfw's EEEM build
 * (github.com/naturarum/dn2-cfw, notes/MR03-eeem.md), whose DSP runs MRB (room through hall to spring on one knob) in
 * the reverb slot. The stock reverb's page (SEND FX 2, page record 18), knob by knob; every knob keeps its row, k,
 * MIDI CC, NRPN, storage and DSP frame word; the title stays "Reverb":
 *
 *   A PRE   row 0x7b k 41  "Pre-delay"   stock names; new-kit default 16 -> 0
 *   B DEC   row 0x7c k 42  "Decay Time"  stock names; value text the plain number (stock: value + 1 and "INF" at
 *                                         127); new-kit default 31 -> 64
 *   C CHAR  row 0x7d k 43  "Character"   was FREQ "FB Shelving Freq"; room (0) through hall to spring (127); default
 *                                         64, as stock
 *   D SIZE  row 0x7e k 44  "Size"        was GAIN "FB Shelving Gain"; new-kit default 50 -> 64
 *   E HPF   row 0x7f k 45  "Input HPF"   stock names; new-kit default 32 -> 0
 *   F TONE  row 0x80 k 46  "Tone"        was LPF "Input LPF"; new-kit default 96 -> 70
 *   G (empty, row 0x83 k 48 untouched), H (empty), LEVEL VOL: stock.
 * C and D are drawn as plain knobs instead of the stock shelf picture (the switch-table and page-id sites of
 * spring-names 0.2). Data only (mod.json's sites, written by dn2-cfw's scripts/mr03_make_mod.py, which checks every
 * stock value first); this file holds the new names. Names repoint to these strings; no stock string is edited.
 */
const char mrn_char[] = "Character", mrn_char_s[] = "CHAR";
const char mrn_size[] = "Size", mrn_size_s[] = "SIZE";
const char mrn_tone[] = "Tone", mrn_tone_s[] = "TONE";
