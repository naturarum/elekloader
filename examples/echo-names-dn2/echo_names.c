/* SPDX-License-Identifier: GPL-2.0-or-later */
/* echo-names (Digitone II OS 1.12): the delay page as the tape echo, on one page, for dn2-cfw's TE01
 * build and the packs after it, whose DSP runs the tape echo (RE-201 style) in the delay slot. It is
 * fx-echo 0.2 (SC01, on a unit) without fx-echo's 13 flanger sites and strings, so it links with
 * flanger-names, which names the chorus slot: every site, string and line of code here is fx-echo
 * 0.2's. EXPERIMENT (dn2-cfw (github.com/naturarum/dn2-cfw), notes/TE01-tape-echo.md; the effect:
 * notes/T5.2-tape-echo.md; the G slot: notes/T6.1-table-growth-study.md section 4.3).
 *
 * The delay page (SEND FX 1, page record 17), knob by knob. Every knob keeps its row, k, MIDI CC,
 * NRPN, storage and frame word; names, two ranges, two formatters, two glyphs and the filter
 * picture change:
 *
 *   A TIME  row 0x71 k 32  "Echo Time"    repeat rate (head 1), the stock tempo-synced law
 *   B MODE  row 0x72 k 33  "Echo Mode"    was X "Pingpong" 0..1 ON/OFF; now 0..6 = heads 1, 2, 3,
 *                                          1+2, 2+3, 1+3, 1+2+3 (value text 1 2 3 12 23 13 123,
 *                                          fxe_fmt_mode); glyph: the numbered box of FM ALGO
 *                                          (row 0xc8), so it shows 1..7, the RE-202's mode numbers
 *   C WID   row 0x73 k 34  "Width/Pan"    shown -64..+63 as stock: < 0 ALT, 0 mono, > 0 RND
 *   D FDBK  row 0x74 k 35  "Intensity"
 *   E CNTR  row 0x75 k 36  "Band Center"  was HPF   } the stock filter picture over E and F
 *   F BW    row 0x76 k 37  "Bandwidth"    was LPF   } draws the band (fxe_band); BW 127 = no filter
 *   G AGE   row 0x7a k 40  "Echo Age"     the stock row "Delay FX Routing" (0..1 Pre/Post, on no
 *                                          page, no CC, stored as kit id 16, sent in frame word
 *                                          0xa18, which the stock DSP never reads): now 0..127, a
 *                                          plain number, a knob glyph, and placed on the empty G
 *                                          slot. Every existing kit holds 1 there (its default).
 *   H REV, LEVEL VOL: stock.
 * MODE and AGE stay never p-lockable: their display flags (8 and 0xc) are not touched, and a lock
 * on an FX row would be stored as a lock on the track's sound slot with the same k. Neither gets a
 * new MIDI CC: MODE keeps the X row's CC 22 and NRPN 257, which reach the value unscaled (CC value
 * v sets v, clamped to 0..6; MIDI out sends the value), AGE keeps none.
 *
 * Data (mod.json's sites, fx-echo 0.2's as dn2-cfw's scripts/t53_make_mod.py wrote them, each stock
 * pointer, value and instruction checked again by scripts/te01_make_echo.py):
 * - `ptr` sites on descriptor rows (0x401f83a4 + 0x3c * row: +0x28 long name, +0x30 short label,
 *   +0x34 formatter) and the modulation destination lists' group name for the delay (0x401f7b04);
 * - `bytes` sites for the two new maxima (+0x0c, << 8) and AGE's formatter (the stock plain number,
 *   0x400e3030, in place of Pre/Post 0x400e3592);
 * - `keep2` sites on the `pea`s that build page 17's names at boot ("Echo", "ECH") and that draw
 *   the SEND FX page box ("ECH": "ECHO" is 15 px in the box font, the box holds 13).
 * With param-grow (GT01) the descriptor table is copied to RAM at boot from the image as loaded,
 * these sites included.
 *
 * Code:
 * - fxe_fmt_mode, MODE's formatter (descriptor +0x34). The OS calls a formatter as
 *   f(value << 8, char *out) from ParameterSet::vslot23 (a tail jump) and from 0x400c25a0; the
 *   stock ones end in sprintf, so this one returns the length as sprintf does.
 * - fxe_graphid (graph.s), at 0x4006f710 in FxPageView's draw (FxPageView::vslot04): the page-id
 *   call before the delay page's filter picture over knobs E and F. The stock picture is a pass
 *   band drawn from two values, a high-pass corner and the flat top's length after it; for the
 *   delay page fxe_graphid hands fxe_band those two values (CNTR's and BW's, as the draw loop kept
 *   them) and fxe_band turns them into the tape echo band's lower edge and width, by the DSP's
 *   own law (band_table.h, written by dn2-cfw's scripts/te01_make_echo.py from fx/spaceecho.c).
 *   CENTER spans 8 octaves over 0..127 and so does the picture, so the band is drawn to scale;
 *   BW 127 (no filter) draws the whole width flat.
 * - fxe_tick, on ev_tick until done: the page record and the display table are built in RAM by the
 *   static initializer 0x400c2c4e at boot (dn2-cfw notes/T6.1 section 4.3); a static edit cannot
 *   put a row id into the record (the stock instruction there is a clr.l). So, on the first tick
 *   that finds them built, after checking they are stock:
 *     1. MODE's glyph: row 0x72's display entry +0x14..+0x23 (the std::function that draws the
 *        glyph: functor, manager, invoker) <- FM ALGO's (row 0xc8: 8 numbered boxes, frame = value);
 *     2. AGE's encoder and glyph: row 0x7a's +0x04..+0x23 (steps per detent, the acceleration
 *        curve, the glyph) <- Band Center's (row 0x75: the plain 0..127 knob). The two
 *        std::functions at +0x24 and +0x34 stay row 0x7a's own, and so do its flags (+0x00) but
 *        bit 2 (0.2): 0xc -> 0x8, as every other delay row. Bit 2 makes the knob draw
 *        (ParameterPageView::vslot45 0x4006448a, MultiSourcePageView::vslot45, MidiParameterPageView::
 *        vslot04, the only readers, through 0x400372fe) keep the label while the knob turns, which
 *        suits the stock two-state routing row but hid AGE's value (SE01's known issue); bit 3 (never
 *        p-lockable, ParameterPageView::vslot35 0x40064ccc reads bits 0 and 3) is kept;
 *     3. page 17's knob G (0x42433f0c, 0 in stock) <- 0x7a, last, so the page never shows the row
 *        before its entry is ready.
 *   If anything is not as stock 1.12 builds it, nothing is written and fxe_state is -1 (the page
 *   then stays FTN1's with Echo names, the G slot empty).
 */
typedef unsigned int u32;

#define DISP 0x4243425cu            /* display table, 0x44 bytes per row (accessor 0x400c257c) */
#define DROW 0x44
#define PAGES 0x42433c00u           /* page records, 0x2c bytes each (accessor 0x400c25d8) */
#define PAGE_DELAY 17

/* stock 1.12 values the tick checks before it writes */
#define INV_X       0x400bc578u     /* row 0x72's glyph invoker: the ON/OFF switch */
#define INV_ROUTING 0x400bc72eu     /* row 0x7a's: PRE FX / POST FX */
#define INV_ALGO    0x400bbee4u     /* row 0xc8's: 8 numbered boxes, frame = value */
#define INV_KNOB    0x400c0986u     /* row 0x75's: the plain knob */
#define CURVE_KNOB  0x44508e40u     /* row 0x75's acceleration curve (also WID, LPF) */

struct pagerec {                    /* 0x2c bytes */
    const char *sname, *lname;      /* COW std::string data pointers */
    int rows[8];                    /* knobs A..H, 0 = empty */
    int level;
};

/* the delay slot: the echo. Widths in the 4 x 5 label font (the knob labels and values): ECHO
 * 19 px (FDBK and WDTH are 19), LECH 18, RECH 19, MODE 20 (MOVD and SR.RT are 20), CNTR 18,
 * BW 10, AGE 14; in the 3 x 6 box font ECH 11 px (the box holds 13). */
const char fxe_echo[] = "Echo";                   /* SEND FX 1 title: "Echo (1/3)" */
const char fxe_ech[] = "ECH";                     /* page box and short name */
const char fxe_echo_s[] = "ECHO";                 /* sends' label, destination group */
const char fxe_echo_send[] = "Echo Send";         /* track FX E, flanger page G */
const char fxe_echo_mix[] = "Echo Mix Vol.";      /* MIXER, FX MIX page */
const char fxe_echo_in[] = "In Echo Send";        /* external input sends */
const char fxe_echo_in_l[] = "In Echo Send L";
const char fxe_echo_in_r[] = "In Echo Send R";
const char fxe_echo_l[] = "LECH";
const char fxe_echo_r[] = "RECH";

/* the delay page's knobs */
const char fxe_time[] = "Echo Time";
const char fxe_mode[] = "Echo Mode", fxe_mode_s[] = "MODE";
const char fxe_wid[] = "Width/Pan";
const char fxe_int[] = "Intensity";
const char fxe_cntr[] = "Band Center", fxe_cntr_s[] = "CNTR";
const char fxe_bw[] = "Bandwidth", fxe_bw_s[] = "BW";
const char fxe_age[] = "Echo Age", fxe_age_s[] = "AGE";

/* MODE 0..6: the heads that play */
static const char mode_text[7][4] = {"1", "2", "3", "12", "23", "13", "123"};

int fxe_fmt_mode(int value, char *out)
{
    int i = value >> 8;
    const char *s = (i >= 0 && i < 7) ? mode_text[i] : "ERR";
    int n = 0;
    while ((out[n] = s[n]) != 0)
        n++;
    return n;
}

/* The band filter's picture (called by fxe_graphid, graph.s, for the delay page only).
 * In: *lo = CNTR, *wid = BW, as the draw loop kept them (value << 8). Out: the band's lower edge
 * and its width, in the same units, for the stock picture (a high-pass corner at *lo, then a flat
 * top *wid long, then a low-pass corner). The DSP's law (fx/spaceecho.c): centre 50 Hz x 2^(8 x
 * CNTR / 127), width 0.5 x 40^(BW / 127) octaves, edges at centre x 2^(-+width / 2); the picture
 * spans 0..127 = the same 8 octaves, so an edge is CNTR -+ band_half[BW] (1/256 knob steps). BW 127
 * has no filter: the whole width, flat. Anything that is not a knob value is left as it is. */
#include "band_table.h"

int fxe_band_seen[2];               /* the last CNTR and BW read (for the emulator test) */

void fxe_band(int *lo, int *wid)
{
    int c = *lo, b = *wid, h, l, u;
    fxe_band_seen[0] = c;
    fxe_band_seen[1] = b;
    if (c < 0 || c > 127 << 8 || b < 0 || b > 127 << 8)
        return;
    if (b >> 8 >= 127) {
        *lo = 0;
        *wid = 127 << 8;
        return;
    }
    h = band_half[b >> 8];
    l = c - h;
    u = c + h;
    if (l < 0)
        l = 0;
    if (u > 127 << 8)
        u = 127 << 8;
    *lo = l;
    *wid = u - l;
}

/* 0 = waiting for the tables, 1 = done, -1 = refused (not stock); for the emulator test */
int fxe_state;
int fxe_ticks;

static volatile u32 *disp(int row)
{
    return (volatile u32 *)(DISP + DROW * row);
}

#define COPY(to, from, w) ((to)[w] = (from)[w])

void fxe_tick(void *ctrl)
{
    volatile struct pagerec *p = (volatile struct pagerec *)(PAGES + 0x2c * PAGE_DELAY);
    volatile u32 *mode = disp(0x72), *algo = disp(0xc8), *age = disp(0x7a), *knob = disp(0x75);
    (void)ctrl;
    if (fxe_state)
        return;
    fxe_ticks++;
    if (!p->rows[0] || !mode[8] || !algo[8] || !age[8] || !knob[8])
        return;                                 /* not built yet: try again on the next tick */
    if (p->rows[0] != 0x71 || p->rows[1] != 0x72 || p->rows[2] != 0x73 || p->rows[3] != 0x74
        || p->rows[4] != 0x75 || p->rows[5] != 0x76 || p->rows[6] != 0 || p->rows[7] != 0x77
        || p->level != 0x79
        || mode[8] != INV_X || algo[8] != INV_ALGO || age[8] != INV_ROUTING || knob[8] != INV_KNOB
        || knob[4] != CURVE_KNOB || !(mode[0] & 8) || !(age[0] & 8)) {
        fxe_state = -1;
        return;
    }
    /* Word by word, unrolled: as a loop, m68k-elf-gcc 16.2 -O2 makes `move.l (a0)+,(340,a0)` of
     * the AGE copy; GCC takes that destination from a0 before the increment, unicorn/QEMU after it
     * (one word off in the emulator). Unrolled, every load and store has its own address. */
    COPY(mode, algo, 5); COPY(mode, algo, 6);   /* MODE: the numbered-box glyph (+0x14..+0x23) */
    COPY(mode, algo, 7); COPY(mode, algo, 8);
    COPY(age, knob, 1); COPY(age, knob, 2);     /* AGE: the knob's steps, curve and glyph */
    COPY(age, knob, 3); COPY(age, knob, 4);     /* (+0x04..+0x23) */
    COPY(age, knob, 5); COPY(age, knob, 6);
    COPY(age, knob, 7); COPY(age, knob, 8);
    age[0] = age[0] & ~4u;                      /* 0.2: AGE's value text while turning (bit 3 kept) */
    p->rows[6] = 0x7a;                          /* AGE on knob G */
    fxe_state = 1;
}
