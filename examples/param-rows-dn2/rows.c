/* SPDX-License-Identifier: GPL-2.0-or-later */
/* param-rows (Digitone II OS 1.12): the 20 parameter rows of the 3-band master compressor,
 * through param-grow's table param_rows. EXPERIMENT: emulator-tested only (dn2-cfw
 * (github.com/naturarum/dn2-cfw) notes/T6.5-table-growth.md for 0.1, GT01;
 * notes/T6.4-compressor-map-and-plan.md section 2.2 for 0.2, CMP1; notes/T6.3-cmp3.md for 0.3, CMP3;
 * notes/FL02.md for 0.4, FL02).
 *
 * The design (the owner's): three pages, one per band (LOW, MID, HIGH), each with threshold,
 * ratio, attack, release and makeup; the two crossovers on the LOW and MID pages; the stock
 * compressor's own rows SCS, MIX and VOL shared by the three pages. comp-ui-dn2 puts these rows
 * on those pages; a later mod (comp-link, CMP2) sends their values to the DSP.
 *
 * 0.1 (GT01) kept the rows inert: default 0, range 0..127, plain numbers, placeholder names.
 * 0.2 (CMP1) changes only data and the value texts, so the image below the stock end is still
 * GT00's: the long names, the ranges, the new-kit defaults (the stock compressor's, per band) and
 * the formatters:
 *
 *   row          k   name (header)       label  range   default        value text
 *   0x141        70  Low Threshold       THR    0..127  32             plain number (stock)
 *   0x142        71  Low Ratio           RAT    0..8    4 (4.00)       1.00 (band off), then the
 *                                                                      stock 8: 1.50 2.00 3.00 4.00
 *                                                                      6.00 8.00 16.00 20.00
 *   0x143        72  Low Attack          ATK    0..127  24             plain number
 *   0x144        73  Low Release         REL    0..127  32             plain number
 *   0x145        74  Low Makeup          MUP    0..127  64 (12.1 dB)   the stock makeup text (dB)
 *   0x146        75  Low/Mid X-Over      XOVR   0..127  68 (196 Hz)    30Hz .. 999Hz, 1.0k
 *   0x147-0x14c  76-81  Mid ... (the same six; Mid/High X-Over default 44 = 2.6k, 1.0k .. 16.0k)
 *   0x14d-0x151  82-86  High Threshold, Ratio, Attack, Release, Makeup (as LOW)
 *
 * 0.3 (CMP3) adds three rows after them, a dry/wet mix per band, copied from the stock MIX row 0x9e
 * (k 67): the same range, text (a plain number) and display entry (look 0x9e), new-kit default 0 (dry,
 * as the stock MIX):
 *
 *   0x152        87  Low Mix             MIX    0..127  0              plain number (stock MIX's)
 *   0x153        88  Mid Mix             MIX    0..127  0
 *   0x154        89  High Mix            MIX    0..127  0
 *
 * comp-ui-dn2 0.2 puts each on its band page's H, in place of the stock MIX; comp-link-dn2 sends k 70..96
 * to the DSP (k 87..89 are its tail words 19..21) and, from 0.2, keeps them in the kit.
 *
 * 0.4 (FL02) changes only which stock row's display entry each band knob borrows (the field `look`), so
 * that every band knob draws the stock compressor's glyph for its kind, as the owner asked: threshold the
 * stock THR curve (row 0x96, 15 frames), attack and release the stock slider (0x97, 0x98, 13 frames),
 * ratio the stock RAT steps (0x9a, 8 frames; comp-ui-dn2 0.3 draws them for the 9 values 0..8), makeup
 * the stock MUP dial (0x99, 96 frames, as before) and the mixes the stock DRY/CMP strip (0x9e, as 0.3).
 * The crossovers keep the default dial (0x99): no stock glyph shows a split frequency. A borrowed entry
 * also brings that row's display flags (RAT: 5, the label stays while it turns), encoder feel and FUNC +
 * turn function (THR, ATK, REL, RAT: to the end of the range, as on the stock page; MUP: to the default).
 * Names, ranges, defaults, formatters, k and the stored values are 0.3's: no kit changes.
 *
 * Every row stays as the study's rules want an FX row (notes/T6.1-table-growth-study.md 3.4):
 * - group 0x14 (the compressor's) in the FX set, each with its own k from 70..89, which no stock
 *   row uses; the stock storage keeps only k 25..69, so a stock kit save does not keep these values
 *   (comp-link-dn2 0.2 adds that) and a stock kit load leaves them as they were;
 * - display flags with bit 3 set (and bit 0 from the borrowed row): never p-lockable (a lock on
 *   an FX row would be stored as a lock on the track's sound slot with the same k);
 * - no randomiser (param-grow clears it), no MIDI CC, no NRPN, no modulation flags;
 * - Overbridge ids 0x400 + n, outside stock's 0..387;
 * - each knob borrows the display entry of its stock compressor row (0.4; THR 0x96, ATK 0x97, REL 0x98,
 *   MUP 0x99, RAT 0x9a, MIX 0x9e); the crossovers borrow Makeup Gain's (0x99), the stock default dial
 *   (0.1 to 0.3: every knob but the mixes did, the owner's placeholder glyph);
 * - on no page by param-grow's slot mechanism (comp-ui's page records place them).
 * The defaults matter only where the OS sets a kit's FX words to the row defaults (a new kit,
 * FUN_400e7140); a loaded kit keeps whatever the live words held.
 */
#include "../param-grow-dn2/param_rows.h"
#include "xover_table.h"

#define K0 70                    /* the first FX slot: 70..89 */
#define LOOK 0x99                /* Makeup Gain's display entry: the stock default dial (MUP, XOVR) */
#define LOOK_THR 0x96            /* 0.4: the stock compressor's own rows, for their glyphs */
#define LOOK_ATK 0x97
#define LOOK_REL 0x98
#define LOOK_RAT 0x9a

/* Stock 1.12 addresses (checked by dn2-cfw's scripts/t63b_build.py) */
#define FMT_DB          ((const void *)0x400e3b2e)   /* the makeup text: '%sdB' from 0x401fde4c */
#define RATIO_TEXT      ((const char *const *)0x401fe0e8)  /* the 8 ratio texts, 1.50 .. 20.00 */
#define SPRINTF         ((int (*)(char *, const char *, ...))0x40000e82)

/* The value texts. The OS calls a formatter f(value << 8, char *out) (descriptor +0x34; from
 * ParameterSet::vslot23 and 0x400c25a0) and the stock ones return what sprintf returns. */
int param_rows_fmt_ratio(int value, char *out)
{
    int i = value >> 8;
    if (i <= 0)
        return SPRINTF(out, "1.00");
    if (i > 8)
        i = 8;
    return SPRINTF(out, "%s", RATIO_TEXT[i - 1]);
}

static int knob(int value)
{
    int i = value >> 8;
    return i < 0 ? 0 : i > 127 ? 127 : i;
}

int param_rows_fmt_xlo(int value, char *out)
{
    int hz = xover_lo_hz[knob(value)];
    if (hz < 1000)
        return SPRINTF(out, "%dHz", hz);
    return SPRINTF(out, "%d.%dk", hz / 1000, hz % 1000 / 100);
}

int param_rows_fmt_xhi(int value, char *out)
{
    int dk = xover_hi_dk[knob(value)];
    return SPRINTF(out, "%d.%dk", dk / 10, dk % 10);
}

#define ROWL(n, nm, sn, mx, df, fmt, look) \
    { 0x141 + (n), { 0x14, K0 + (n), V(0), V(mx), V(df), 0, -1, -1, -1, 0x400 + (n), 0, \
                     nm, STR_MASTER, sn, fmt, STR_EMPTY }, look, 8, 0, -1, -1 }
#define ROW(n, nm, sn, mx, df, fmt) ROWL(n, nm, sn, mx, df, fmt, LOOK)
#define LOOK_MIX 0x9e            /* the stock MIX row's display entry */

#define BAND(n, b) \
    ROWL((n) + 0, b " Threshold", "THR", 127, 32, FMT_NUMBER, LOOK_THR), \
    ROWL((n) + 1, b " Ratio", "RAT", 8, 4, param_rows_fmt_ratio, LOOK_RAT), \
    ROWL((n) + 2, b " Attack", "ATK", 127, 24, FMT_NUMBER, LOOK_ATK), \
    ROWL((n) + 3, b " Release", "REL", 127, 32, FMT_NUMBER, LOOK_REL), \
    ROW((n) + 4, b " Makeup", "MUP", 127, 64, FMT_DB)

const struct param_row param_rows_comp[] __attribute__((aligned(4))) = {
    BAND(0, "Low"),
    ROW(5, "Low/Mid X-Over", "XOVR", 127, 68, param_rows_fmt_xlo),
    BAND(6, "Mid"),
    ROW(11, "Mid/High X-Over", "XOVR", 127, 44, param_rows_fmt_xhi),
    BAND(12, "High"),
    ROWL(17, "Low Mix", "MIX", 127, 0, FMT_NUMBER, LOOK_MIX),
    ROWL(18, "Mid Mix", "MIX", 127, 0, FMT_NUMBER, LOOK_MIX),
    ROWL(19, "High Mix", "MIX", 127, 0, FMT_NUMBER, LOOK_MIX),
    { 0 }
};
