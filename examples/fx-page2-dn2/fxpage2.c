/* SPDX-License-Identifier: GPL-2.0-or-later */
/* fx-page2 (Digitone II OS 1.12): a second page for the chorus in the SEND FX view, with 8 new
 * knobs. UI test only: the 8 values live in FX-setup words 70-77, which no page and no DSP frame
 * word uses in stock 1.12 (dn2-cfw (github.com/naturarum/dn2-cfw) notes/T5.0-flanger-page-ui.md, section 0); the DSP
 * never sees them. Emulator-tested only.
 *
 * How a send-FX page is made (all addresses 1.12):
 *  - The SEND FX view (FUNC+FX) is an FxPageView whose pages are a vector of page ids, built in
 *    MainScreenView's constructor from the array {17, 18, 19} at 0x401e0448 (DEL, REV, CHO).
 *    UP/DOWN step through it; the title shows "(i/n)" and the right edge a page marker, both
 *    from the vector's length. Sites: the count (moveq #3 at 0x40061616 -> 4) and the array
 *    (keep2 at 0x40061624 -> fx2_pages = {17, 18, 19, 37}).
 *  - A page id names a 0x2c-byte record: short name, long name (COW std::string data
 *    pointers), the descriptor rows of encoders A-H, the LEVEL row. Stock records live at
 *    0x42433c00 (ids 0-36), reached only through 0x400c25d8: jmp to fx2_pagerec (pagerec.s),
 *    which returns fx2_rec for id 37.
 *  - FxPageView draws the delay/reverb filter graphic unless the page id is 0x13, and its box
 *    label by id: three `jsr` sites replace those page-id calls by fx2_baseid, which reports
 *    37 as 0x13 there.
 *  - Each knob is a parameter row: descriptor (names, range, formatter, mirror index k) in the
 *    image, a display row in RAM (flags and the glyph's std::function). 8 rows that no page
 *    shows ("Error" rows 1-5 and 0xb-0xd) are turned into FX parameters by sites in mod.json;
 *    their display rows are copied from row 0x69 (DPTH: knob) and 0x63 (LFO3 WAVE: the LFO
 *    waveform glyph, for SHPE) on each tick until they match. Those 8 rows carry MIDI CCs in
 *    stock (1, 2, 6, 7, 10, 99, 120, 121); the sites clear them, so a real build needs other
 *    rows (see the note).
 */
typedef unsigned int u32;
typedef int (*vfn)(void *);

#define FX2_ID 37
#define DISP 0x4243425cu                  /* display table, 0x44 bytes per row (0x400c257c) */
#define DROW 0x44

struct srep {                             /* libstdc++ COW std::string rep: refcount -1 = leaked, */
    int len, cap, ref;                    /* so every copy clones and nothing ever frees this one */
    char s[12];
};
struct pagerec {
    const char *sname, *lname;
    int rows[8];
    int level;
};

const int fx2_pages[4] = {17, 18, 19, FX2_ID};

static struct srep fx2_short = {4, 4, -1, "FLG2"};
static struct srep fx2_long = {9, 9, -1, "Flanger 2"};
struct pagerec fx2_rec = {fx2_short.s, fx2_long.s,
                          {0x01, 0x02, 0x03, 0x04, 0x05, 0x0b, 0x0c, 0x0d}, 0x70};

/* the display row each new knob copies: SHPE gets the LFO waveform glyph, the rest a knob */
static const unsigned short src_row[8] = {0x63, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69};

const char fx2_g[] = "Flanger";
const char fx2_l0[] = "LFO Shape", fx2_s0[] = "SHPE";
const char fx2_l1[] = "LFO Phase", fx2_s1[] = "PHAS";
const char fx2_l2[] = "Rate Multiplier", fx2_s2[] = "MULT";
const char fx2_l3[] = "Tempo Sync", fx2_s3[] = "SYNC";
const char fx2_l4[] = "Stereo Width", fx2_s4[] = "WDTH";
const char fx2_l5[] = "Feedback Tone", fx2_s5[] = "TONE";
const char fx2_l6[] = "Drive", fx2_s6[] = "DRIV";
const char fx2_l7[] = "Negative Feedback", fx2_s7[] = "NEG";

int fx2_copies;                           /* display rows written (for the emulator test) */

/* jsr sites in FxPageView: the page id, with the new page reported as the chorus (0x13) */
int fx2_baseid(void *view)
{
    vfn f = (*(vfn **)view)[0x88 / 4];
    int id = f(view);
    return id == FX2_ID ? 0x13 : id;
}

void fx2_tick(void *ctrl)
{
    int i, j;
    (void)ctrl;
    for (i = 0; i < 8; i++) {
        const u32 *src = (const u32 *)(DISP + DROW * src_row[i]);
        u32 *dst = (u32 *)(DISP + DROW * fx2_rec.rows[i]);
        if (src[0x1c / 4] == 0)           /* the source's glyph function is not built yet */
            continue;
        for (j = 0; j < DROW / 4; j++)
            if (dst[j] != src[j])
                break;
        if (j == DROW / 4)
            continue;
        for (j = 0; j < DROW / 4; j++)
            dst[j] = src[j];
        fx2_copies++;
    }
}
