/* SPDX-License-Identifier: GPL-2.0-or-later */
/* param-rows (Digitone II OS 1.12): 17 parameter rows kept for a 3-band master compressor,
 * through param-grow's table param_rows. EXPERIMENT: emulator-tested only (dn2-cfw
 * (github.com/naturarum/dn2-cfw) notes/T6.5-table-growth.md). For dn2-cfw's GT01 build.
 *
 * The plan (the owner's): three pages, one per band (LOW, MID, HIGH), each with threshold,
 * ratio, attack, release and makeup; the crossovers on the LOW and MID pages; the stock
 * compressor's own rows reused where they fit. These rows are that plan's knobs, and nothing
 * yet: inert until a later mod puts them on pages and gives them a DSP.
 *
 * Inert, row by row (notes/T6.1-table-growth-study.md section 3.4):
 * - group 0x14 (the compressor's) in the FX set, each with its own k from 70..86. Those k
 *   are free in the FX set (stock rows use 25..69), so no stock row loses its slot in the
 *   map the OS builds at boot. The stock storage keeps only k 25..69 (stored ids 1..45), so
 *   these values are not saved: a kit that has them carries nothing new.
 * - default 0, as stock leaves every FX word it has no row for: a new kit is unchanged;
 * - display flags with bit 3 set (and bit 0 from the borrowed row): never p-lockable;
 * - no randomiser (param-grow clears it), no MIDI CC, no NRPN, no modulation flags;
 * - Overbridge ids 0x400 + n, outside stock's 0..387;
 * - on no page.
 * Ranges, names and formatters are placeholders until the compressor is designed.
 */
#include "../param-grow-dn2/param_rows.h"

#define K0 70                    /* the first FX slot: 70..86 */
#define LOOK 0x99                /* Makeup Gain's display entry: the plain knob */

#define COMP(n, nm, sn) \
    { 0x141 + (n), { 0x14, K0 + (n), V(0), V(127), V(0), 0, -1, -1, -1, 0x400 + (n), 0, \
                     nm, STR_MASTER, sn, FMT_NUMBER, STR_EMPTY }, LOOK, 8, 0, -1, -1 }

const struct param_row param_rows_comp[] __attribute__((aligned(4))) = {
    COMP(0, "Low Threshold", "THR"),
    COMP(1, "Low Ratio", "RAT"),
    COMP(2, "Low Attack", "ATK"),
    COMP(3, "Low Release", "REL"),
    COMP(4, "Low Makeup", "MUP"),
    COMP(5, "Low X-Over", "XOVR"),
    COMP(6, "Mid Threshold", "THR"),
    COMP(7, "Mid Ratio", "RAT"),
    COMP(8, "Mid Attack", "ATK"),
    COMP(9, "Mid Release", "REL"),
    COMP(10, "Mid Makeup", "MUP"),
    COMP(11, "Mid X-Over", "XOVR"),
    COMP(12, "High Threshold", "THR"),
    COMP(13, "High Ratio", "RAT"),
    COMP(14, "High Attack", "ATK"),
    COMP(15, "High Release", "REL"),
    COMP(16, "High Makeup", "MUP"),
    { 0 }
};
