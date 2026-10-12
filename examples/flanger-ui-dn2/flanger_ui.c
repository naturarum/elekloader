/* SPDX-License-Identifier: GPL-2.0-or-later */
/* flanger-ui (Digitone II OS 1.12): the chorus page and the chorus sends, renamed and redrawn
 * as a flanger. UI only: the DSP still runs the stock chorus, and every knob still sends the
 * same frame word as before. A test of how the Digitone II draws its pages (dn2-cfw (github.com/naturarum/dn2-cfw)
 * notes/T5.0-flanger-page-ui.md); emulator-tested only.
 *
 * Two mechanisms:
 *
 * 1. Names (mod.json "sites", no code): the parameter descriptor table at 0x401f83a4 (rows of
 *    0x3c bytes) holds plain char pointers, read whenever a page is drawn: +0x28 the long name
 *    (the header's "Name=value"), +0x2c the group, +0x30 the short label under the knob. `ptr`
 *    sites point them at the strings below. The page title ("Chorus (3/3)") and the page box
 *    ("CHO") come from string literals that a static initializer and the FX page's box routine
 *    push with `pea`: `keep2` sites point those at "Flanger" and "FLG".
 *
 * 2. Glyphs (code, on the first ev_tick): a parameter's glyph is a frame of a "strip", a
 *    std::vector<Bitmap> ({begin, end, cap}) at a fixed address, built at boot by the static
 *    initializer 0x40151b94 from pixel data in the image. The draw routine of each parameter
 *    picks the frame from the value (0x40115e3e) and blits it (0x40115adc). Pointing a strip's
 *    vector at Bitmaps of our own replaces the glyph everywhere that strip is drawn, with any
 *    number of frames. The stock vectors' heap storage is left as it is.
 *
 * Bitmap (0x1c bytes): vtable 0x402069c4, width, height, stride (u32 words per column),
 * pixel words, mask words, 0. Column-major; pixel y (0 = the bottom row) is bit
 * 0x80000000 >> y; the blit writes the pixel where the mask bit is 1.
 */
#include "flanger_art.h"

typedef unsigned int u32;

struct bitmap {
    u32 vt;
    int w, h, stride;
    const u32 *data, *mask;
    int zero;
};

struct strip {                            /* std::vector<Bitmap> */
    struct bitmap *begin, *end, *cap;
};

#define BMP_VT 0x402069c4u

/* The strips this mod replaces (addresses in Digitone II 1.12), each with the stock frame
 * count and width checked before it is touched. */
#define STRIP_CHORUS_SEND 0x44646bdcu     /* 13 frames, 19 x 17: track CHR, external CHR sends */
#define STRIP_CHORUS_HPF  0x446466d8u     /* 12 frames, 19 x 17: chorus page C (HPF) only */
#define STRIP_CHORUS_WDTH 0x4464642cu     /* 12 frames, 19 x 17: chorus page D (WDTH) only */

/* Names. The 4 x 5 label font fits 5 characters under a knob; the title font is proportional. */
const char flui_title[] = "Flanger";      /* page title: "Flanger (3/3)", and the group name */
const char flui_box[] = "FLG";            /* the box at the left of the page */
const char flui_send[] = "Flanger Send";
const char flui_send_s[] = "FLG";
const char flui_mix[] = "Flanger Mix Vol.";
const char flui_in[] = "In Flanger Send";
const char flui_in_l[] = "In Flanger Send L";
const char flui_in_l_s[] = "LFLG";
const char flui_in_r[] = "In Flanger Send R";
const char flui_in_r_s[] = "RFLG";
const char flui_fdbk[] = "Feedback";
const char flui_fdbk_s[] = "FDBK";
const char flui_time[] = "Delay Time";
const char flui_time_s[] = "TIME";

#define FRAME(set, k) {BMP_VT, FLUI_W, FLUI_H, 1, set[k], FLUI_MASK, 0}

static struct bitmap send_frames[FLUI_SEND_N] = {
    FRAME(FLUI_SEND, 0), FRAME(FLUI_SEND, 1), FRAME(FLUI_SEND, 2), FRAME(FLUI_SEND, 3),
    FRAME(FLUI_SEND, 4), FRAME(FLUI_SEND, 5), FRAME(FLUI_SEND, 6), FRAME(FLUI_SEND, 7),
    FRAME(FLUI_SEND, 8), FRAME(FLUI_SEND, 9), FRAME(FLUI_SEND, 10), FRAME(FLUI_SEND, 11),
    FRAME(FLUI_SEND, 12),
};
static struct bitmap fdbk_frames[FLUI_FDBK_N] = {
    FRAME(FLUI_FDBK, 0), FRAME(FLUI_FDBK, 1), FRAME(FLUI_FDBK, 2), FRAME(FLUI_FDBK, 3),
    FRAME(FLUI_FDBK, 4), FRAME(FLUI_FDBK, 5), FRAME(FLUI_FDBK, 6), FRAME(FLUI_FDBK, 7),
    FRAME(FLUI_FDBK, 8), FRAME(FLUI_FDBK, 9), FRAME(FLUI_FDBK, 10), FRAME(FLUI_FDBK, 11),
};
static struct bitmap time_frames[FLUI_TIME_N] = {
    FRAME(FLUI_TIME, 0), FRAME(FLUI_TIME, 1), FRAME(FLUI_TIME, 2), FRAME(FLUI_TIME, 3),
    FRAME(FLUI_TIME, 4), FRAME(FLUI_TIME, 5), FRAME(FLUI_TIME, 6), FRAME(FLUI_TIME, 7),
    FRAME(FLUI_TIME, 8), FRAME(FLUI_TIME, 9), FRAME(FLUI_TIME, 10), FRAME(FLUI_TIME, 11),
};

/* 0 = nothing done yet, 1 = all three strips replaced, -1 = a strip was not as expected */
int flui_state;
int flui_ticks;

/* -> 1 replaced, 0 not built yet (try again next tick), -1 not the expected strip */
static int replace(u32 at, int stock_n, struct bitmap *frames, int n)
{
    struct strip *v = (struct strip *)at;
    if (v->begin == 0)
        return 0;
    if (v->begin == frames)
        return 1;
    if (v->end - v->begin != stock_n || v->begin->vt != BMP_VT || v->begin->w != FLUI_W
        || v->begin->h != FLUI_H || v->begin->stride != 1)
        return -1;
    v->begin = frames;
    v->end = frames + n;
    v->cap = frames + n;
    return 1;
}

void flui_tick(void *ctrl)
{
    int a, b, c;
    (void)ctrl;
    if (flui_state)
        return;
    flui_ticks++;
    a = replace(STRIP_CHORUS_SEND, 13, send_frames, FLUI_SEND_N);
    b = replace(STRIP_CHORUS_HPF, 12, fdbk_frames, FLUI_FDBK_N);
    c = replace(STRIP_CHORUS_WDTH, 12, time_frames, FLUI_TIME_N);
    if (a < 0 || b < 0 || c < 0)
        flui_state = -1;
    else if (a && b && c)
        flui_state = 1;
}
