/* SPDX-License-Identifier: GPL-2.0-or-later */
/* comp-ui (Digitone II OS 1.12): the 3-band master compressor's pages in the MIXER view
 * (FUNC+MOD), for dn2-cfw's CMP1 and CMP3 builds. EXPERIMENT: emulator-tested only (dn2-cfw
 * (github.com/naturarum/dn2-cfw) notes/T6.4-compressor-map-and-plan.md sections 1.6 and 2.1,
 * stage CMP1 in section 5; notes/T6.3-cmp3.md for 0.2). Needs core-dn2 0.2 (page records), param-grow
 * and param-rows 0.3 (the band rows 0x141..0x154, their ranges and value texts; mod.json requires
 * param-rows>=0.3, so the patcher refuses param-rows 0.1 and 0.2).
 *
 * The MIXER view's pages: stock {20 Compressor, 21 INT MIX 1, 22 INT MIX 2, 23 FX MIX,
 * 24 EXT MIX 1} (+ 25 EXT MIX 2 in DUAL mode, + 26 Outbox 8); here {40 LOW, 41 MID, 42 HIGH,
 * 21, 22, 23, 24} (+ 25, + 26). The stock Compressor page (20) leaves the list; its record and
 * rows stay as they are. Each band page, in the stock Compressor page's layout:
 *
 *   page             title                  box        A    B    C    D    E    F        G    H    LEVEL
 *   LOW  (id 40)     Compressor Low (1/7)   CMP / LOW  THR  ATK  REL  MUP  RAT  XOVR lo  SCS  MIX  VOL
 *   MID  (id 41)     Compressor Mid (2/7)   CMP / MID  THR  ATK  REL  MUP  RAT  XOVR hi  SCS  MIX  VOL
 *   HIGH (id 42)     Compressor High (3/7)  CMP / HI   THR  ATK  REL  MUP  RAT  (empty)  SCS  MIX  VOL
 *
 * A..F are each band's own rows (param-rows); 0.2: H is the band's own dry/wet mix (param-rows 0.3's
 * 0x152, 0x153, 0x154: Low, Mid, High Mix), in place of the stock MIX 0x9e that 0.1 shared, which
 * is then on no page (still saved, still on MIDI CC 118 / NRPN 279). G and LEVEL are the stock rows
 * 0x9b SCS (0x9c in DUAL mode) and 0x9f VOL, the same value on all three pages. The FX Mixer page
 * stays stock (SCF, row 0x9d, is on no page: the owner dropped it).
 *
 * The sites (mod.json; each stock instruction is checked by the SDK):
 * - 0x4006169c `moveq #5,d1` -> `moveq #7,d1` and 0x400616aa `move.l #0x401e0434,d0` (keep2) ->
 *   compui_pages: MainScreenView's constructor builds the MIXER view's page list from that array;
 * - 0x40070b84 `lea (0x14,a1),a1` -> `lea (0x1c,a1),a1`: DUAL on inserts EXT MIX 2 (25) at a fixed
 *   index, stock 5 (after EXT MIX 1), here 7 (DUAL off erases it by value, OB8 is pushed and
 *   erased at the end, by value);
 * - 0x4007093e, MasterPageView::vslot04's default case `jmp 0x40065a0c` (ids outside 20..26) ->
 *   compui_draw: the stock page drawing, then for 40..42 the box and the band's GR bar;
 * - 0x4006ffc8 in MasterPageView::vslot47 (the row behind a knob) -> compui_baseid (baseid.s):
 *   40..42 answer 0x14 to its page test, so G shows SCS's DUAL row 0x9c as on the stock page.
 * The records go to core's table core_pagerecs (ids 40..42, claimed as page:40..42).
 *
 * The GR bars: the DSP (CMP0's, and later) writes Q15 (1 - g) of the low, mid and high band into
 * reply half-words 8, 9 and 10 (bytes +0x10..+0x15), which land at 0x80001a30/32/34 in the
 * ColdFire's receive buffer; the stock DSP never writes them and the stock OS reads them nowhere
 * live (dn2-cfw notes/T6.4 2.3). Each band page reads its own half-word and draws it as the stock
 * Compressor page draws half-word 11 (FUN_400703f2): h = q / 655 px, nothing when h <= 0, a 2-px
 * bar at x 18-19 from y 50 down to y 51 - h (y up). This mod only reads those bytes. It claims them
 * as dsp-reply:0x10-0x15, a numbered claim: elekloader refuses any other mod whose dsp-reply claim
 * shares a byte with it (dsp-reply:0x04-0x13, say), so that no other mod gives them another meaning.
 *
 * In .run (DDR), all of it: the constructor reads the page list before the first ev_tick, and the
 * draw runs inside the stock drawAll.
 */
typedef int (*vfn)(void *);

struct srep {                       /* a COW std::string's rep: length, capacity, refcount, text */
    int len, cap, ref;
    char s[20];
};
struct pagerec {                    /* 0x2c bytes: the stock layout */
    const char *sname, *lname;      /* COW std::string data pointers */
    int rows[8];                    /* knobs A..H, 0 = empty */
    int level;
};

/* refcount -1: an unshareable ("leaked") string: the OS copies the text out and never frees it */
#define SREP(str) { sizeof(str) - 1, sizeof(str) - 1, -1, str }
static struct srep s_low = SREP("COMP LOW"), s_mid = SREP("COMP MID"), s_high = SREP("COMP HIGH");
static struct srep l_low = SREP("Compressor Low"), l_mid = SREP("Compressor Mid"),
                   l_high = SREP("Compressor High");

#define SCS 0x9b
#define VOL 0x9f
#define MIX_L 0x152                 /* param-rows 0.3: Low, Mid, High Mix */
#define MIX_M 0x153
#define MIX_H 0x154
struct pagerec compui_rec[3] __attribute__((aligned(4))) = {
    /*                    A THR  B ATK  C REL  D MUP  E RAT  F XOVR  G    H        LEVEL */
    { s_low.s,  l_low.s,  { 0x141, 0x143, 0x144, 0x145, 0x142, 0x146, SCS, MIX_L }, VOL },
    { s_mid.s,  l_mid.s,  { 0x147, 0x149, 0x14a, 0x14b, 0x148, 0x14c, SCS, MIX_M }, VOL },
    { s_high.s, l_high.s, { 0x14d, 0x14f, 0x150, 0x151, 0x14e, 0,     SCS, MIX_H }, VOL },
};

/* the MIXER view's pages, in order (MainScreenView's constructor copies 7 of them) */
const int compui_pages[7] __attribute__((aligned(4))) = { 40, 41, 42, 21, 22, 23, 24 };

#define PPV_DRAW ((void (*)(void *, void *))0x40065a0c)                  /* ParameterPageView::vslot04 */
#define FILLRECT ((void (*)(void *, int, int, int, int, int))0x40114b22)
#define RECT     ((void (*)(void *, int, int, int, int, int))0x40114906)
#define VBAR     ((void (*)(void *, int, int, int, int, int))0x401147f0) /* the stock GR bar's */
#define TEXTF    ((void (*)(void *, void *, int, int, int, const char *))0x4011573c)
#define FONT_BOX ((void *)0x44508eb8)                                      /* the 3x6 box font */
#define RX       ((volatile short *)0x80001a20)                            /* the DSP's reply */

static const char t_cmp[] = "CMP", t_low[] = "LOW", t_mid[] = "MID", t_hi[] = "HI";
static const char *const band_txt[3] = { t_low, t_mid, t_hi };

/* for the emulator test: draws of a band page, and the last bar height per band (-1 = none yet) */
int compui_draws;
int compui_bar[3] = { -1, -1, -1 };

static int page_id(void *view)
{
    return (*(vfn **)view)[0x88 / 4](view);
}

/* Replaces the tail `jmp 0x40065a0c` of MasterPageView::vslot04's default case, with its
 * arguments: the stock page drawing first, then for 40..42 what FUN_400703f2 adds for page 20 */
void compui_draw(void *view, void *bmp)
{
    int id = page_id(view), b, q, h;
    PPV_DRAW(view, bmp);
    if (id < 40 || id > 42)
        return;
    b = id - 40;
    /* the box (FUN_400702fa's frame, with two lines as on EXT MIX's) */
    FILLRECT(bmp, 0, 0x1b, 0xe, 0x32, 0);
    TEXTF(bmp, FONT_BOX, 7, 0x28, 2, t_cmp);
    TEXTF(bmp, FONT_BOX, 7, 0x20, 2, band_txt[b]);
    RECT(bmp, 0, 0x1b, 0xe, 0x32, 1);
    /* the band's GR bar, by the stock law */
    q = RX[8 + b];
    h = q / 655;
    if (h > 0)
        VBAR(bmp, 0x12, 0x32, 0x13, 0x33 - h, 1);
    compui_bar[b] = h > 0 ? h : 0;
    compui_draws++;
}
