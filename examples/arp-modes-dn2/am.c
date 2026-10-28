/* SPDX-License-Identifier: GPL-2.0-or-later */
/* arp-modes (Digitone II OS 1.12): the arpeggiator's SHUF, RAND and TM (am.s has the step's sites and registers).
 *
 *   SHUF (MODE 5)  every held note in every octave of RANGE once per cycle, in a new random order each cycle; a cycle
 *                  does not start on the note the one before ended on. More than 64 notes x octaves: as RAND.
 *   RAND (MODE 6)  a random held note in a random octave of RANGE at each step.
 *   TM (MODE 7..107, 0.3)  a Turing Machine loop of notes: one value per step of ARP LENGTH (0..255, random at
 *                  first), put on the notes the loop plays: from the lowest note held up, those of the track's scale
 *                  (KEYBOARD SETUP, as lfo-dejavu's KEY reads it) over the octaves of RANGE, lowest first; the other
 *                  notes held are not used. Before an on step plays, it takes another of those notes with the chance
 *                  LOOP CHANGE / 100, LOOP CHANGE being MODE - 7: 0 locks the loop, 100 makes every step new every
 *                  time round; a change touches that step only. The steps' offsets are not added. (0.2, in BA03, was
 *                  a 16-bit shift register over the notes held: a flipped bit passed through eight notes.)
 * All three take RANGE, the steps' on/off, LEN, SPEED and N.LEN as the stock modes do; SHUF and RAND the notes held (the
 * arp's 128-bit set), TM the lowest of them. SHUF and RAND add the steps' offsets, and their off steps rest without
 * using up a note. Inspired by research findings of dn2_firmware_explore (angellinares) on OS 1.11; TM after Tom
 * Whitwell's Turing Machine (Music Thing Modular); this is our own code. For dn2-cfw (notes/BA02-base-0.2.md,
 * notes/BA03-turing-arp.md, notes/BA04-turing-arp-v2.md).
 *
 * The ARP menu (ArpSetupMenuView, 0.2): MODE's eighth name, CHRD (which no stock menu reaches), reads TM; knob A steps
 * OFF .. RAND, TM and holds there, and comes back into TM with the track's last LOOP CHANGE; in TM, knob E is LOOP
 * CHANGE (0..100) where STEP OFFSET was, and the step graph shows the loop's notes, one bar per step (the place of the
 * step's note among the notes the loop plays, or the step's value with none held), with the playing step marked above
 * the row of squares; the menu's tick redraws it when the loop changes or moves on.
 *
 * The random source is xorshift32, seeded at its first use from a constant and the frame clock 0x80005398 (another
 * order after each power-on); a test can set am_rng beforehand for a fixed sequence. The engine runs in the voice
 * interrupt only (the arp's step), as the stock arp does; the menu only reads the loop (seeding one it has not made).
 */
typedef unsigned long u32;
typedef long s32;
typedef unsigned short u16;
typedef unsigned char u8;

#define HELD  ((volatile u32 *)0x40599728)              /* track t's held notes: 4 words at 4 t; bit b of word w = note 32 w + b */
#define CLOCK ((volatile u32 *)0x80005398)
#define HEADS ((volatile u32 *)0x4059d868)              /* track t's press-order list: zero while no note is held */
#define ARPST ((volatile u8 *)0x405994a8)               /* track t's arp state at 40 t: +0 the step playing */

/* KEYBOARD SETUP's scale and root, in the pattern playing (the sequencer's pointer): track t's at + 0x6a5 t + 0x492 /
 * + 0x494, the pattern's at + 0x1dd40 + 0x1d / + 0x1f, bit 2 of + 0x1dd40 + 0x21 set when they are per track. A scale:
 * -1 Chromatic, 0..34 a row of the stock table (8 notes, -1 after the last). As lfo-dejavu's KEY reads them. */
#define PLAYING   ((u8 *volatile *)0x42c5b9a0)
#define SCALE_TAB ((const s32 *)0x4020e358)

/* the menu's stock helpers (the ARP menu finds its sound the same way) */
#define PROJECT    ((void *(*)(void))0x4018ac5a)
#define P_TRACKS   ((void *(*)(void *))0x4003e40e)
#define P_SOUNDS   ((void *(*)(void *))0x4003e41a)
#define TRACK_OF   ((int (*)(void *))0x40046bfe)
#define SOUND_OF   ((void *(*)(void *, int))0x40030e66)
#define GET_MODE   ((int (*)(void *))0x4004be74)
#define SET_MODE   ((void (*)(void *, int))0x4004bea4)
#define GET_LENM1  ((int (*)(void *))0x4004bba0)
#define GET_RANGE  ((int (*)(void *))0x4004c0aa)
#define STEP_ON    ((int (*)(void *, int))0x4004bd06)
#define GET_OFS    ((int (*)(void *, int))0x4004bc3c)
#define SET_OFS    ((void (*)(void *, int, int))0x4004bc84)
#define MENU_DRAW  ((void (*)(void *, void *))0x40018818)  /* ArpSetupMenuView slot 4 */
#define MENU_TICK  ((void (*)(void *))0x40019a72)          /* ArpSetupMenuView slot 11 */
#define INVALIDATE ((void (*)(void *))0x4011ca9a)
#define FILL       ((void (*)(void *, int, int, int, int, int))0x40114b22)   /* x0, y0, x1, y1 (inclusive), colour */
#define HLINE      ((void (*)(void *, int, int, int, int))0x40113fd4)        /* x0, y, x1, colour */
#define DOTS_V     ((void (*)(void *, int, int, int, int))0x40114346)        /* x, y0, y1, colour: dotted */
#define FONT       ((void *(*)(void *))0x40115fe4)
#define TEXT       ((void (*)(void *, void *, int, int, int, int, const char *, const char *, ...))0x40115948)
#define SMALL      ((void *)0x44508ed8)                 /* the font of the menu's knob labels */
#define T_XXX      ((const char *)0x40217572)           /* "XXX", the value's width */
#define T_D        ((const char *)0x4021517a)           /* "%d" */

/* The screen's y runs up from the bottom row (0) to the top (63). */

struct am_track {
    u32 sig;            /* the held set and RANGE the order was made for */
    u32 id;             /* the arp's id it was made in (a new arp starts a new order) */
    u8 m;               /* notes x octaves in the order */
    u8 pos;             /* the next place */
    u8 last;            /* the last slot played */
    u8 made;            /* 1: an order exists */
    u8 perm[64];
};

struct am_tm {
    u8 v[16];           /* the loop: each step's value, 0..255, put on the notes the loop plays when the step plays */
    u8 made;            /* 1: seeded */
    u8 pad[3];
};

struct am_track am_tr[16] __attribute__((aligned(4)));
struct am_tm am_tml[16] __attribute__((aligned(4)));
u8 am_chg[16];          /* each track's LOOP CHANGE when it last left TM (knob A brings it back) */
u32 am_drawn;           /* what the ARP menu drew last in TM; its tick redraws when the loop has moved since */
u32 am_rng;

const char am_s_tm[] = "TM";
static const char am_s_loop[] = "LOOP";
static const char am_s_change[] = "CHANGE";

static u32 rnd(void)
{
    u32 x = am_rng;
    if (!x) {
        x = 0x2545f491u ^ *CLOCK;
        if (!x)
            x = 0x2545f491u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    am_rng = x;
    return x;
}

static int held_count(const volatile u32 *w)
{
    int i, n = 0;
    for (i = 0; i < 4; i++) {
        u32 x = w[i];
        while (x) {
            x &= x - 1;
            n++;
        }
    }
    return n;
}

/* the k-th held note, lowest first (k below the count) */
static int held_note(const volatile u32 *w, int k)
{
    int note;
    for (note = 0; note < 128; note++)
        if ((w[note >> 5] >> (note & 31)) & 1)
            if (k-- == 0)
                return note;
    return -1;
}

static int clamp_range(int range)
{
    return range < 0 ? 0 : range > 9 ? 9 : range;
}

/* -> the note (0..127) with the octave stored at st + 0x1c, or -1 (a rest) */
int am_pick(int t, u8 *st, const u8 *sound, int mode)
{
    const volatile u32 *w = HELD + 4 * (t & 15);
    int range = (signed char)sound[0x161], n = held_count(w), m, k;

    if (n <= 0)
        return -1;
    range = clamp_range(range);
    m = n * (range + 1);
    if (mode == 6 || m > 64) {
        k = (int)(rnd() % (u32)m);
    } else {
        struct am_track *a = &am_tr[t & 15];
        u32 sig = w[0] ^ (w[1] * 3u) ^ (w[2] * 5u) ^ (w[3] * 7u) ^ ((u32)range << 28);
        u32 id = *(volatile u32 *)(st + 0x18);
        if (!a->made || a->sig != sig || a->id != id || a->m != m || a->pos >= m) {
            int i;
            for (i = 0; i < m; i++)
                a->perm[i] = (u8)i;
            for (i = m - 1; i > 0; i--) {               /* Fisher-Yates */
                int j = (int)(rnd() % (u32)(i + 1));
                u8 x = a->perm[i];
                a->perm[i] = a->perm[j];
                a->perm[j] = x;
            }
            if (a->made && m > 1 && a->perm[0] == a->last) {   /* not the note the cycle before ended on */
                int j = 1 + (int)(rnd() % (u32)(m - 1));
                u8 x = a->perm[0];
                a->perm[0] = a->perm[j];
                a->perm[j] = x;
            }
            a->sig = sig;
            a->id = id;
            a->m = (u8)m;
            a->pos = 0;
            a->made = 1;
        }
        k = a->perm[a->pos++];
        a->last = (u8)k;
    }
    *(volatile u32 *)(st + 0x1c) = (u32)(k / n);
    return held_note(w, k % n);
}

/* ---- TM */

static int is_tm(int mode)
{
    return mode >= 7 && mode <= 107;
}

static int tm_len(int len_m1)
{
    int len = len_m1 + 1;
    return len < 1 ? 1 : len > 16 ? 16 : len;
}

static void tm_seed(struct am_tm *s)
{
    int i;

    if (!s->made) {
        for (i = 0; i < 16; i++)
            s->v[i] = (u8)rnd();
        s->made = 1;
    }
}

/* -> the scale's classes counted from its root (bit c: class c; all twelve for Chromatic or no pattern), the root
 * (0..11) at *root */
static int tm_scale(int t, int *root)
{
    const u8 *p = *PLAYING;
    int sc, rt, i, m = 0;

    *root = 0;
    if (!p)
        return 0xfff;
    if (p[0x1dd40 + 0x21] & 4) {
        sc = (signed char)p[0x6a5 * t + 0x492];
        rt = (signed char)p[0x6a5 * t + 0x494];
    } else {
        sc = (signed char)p[0x1dd40 + 0x1d];
        rt = (signed char)p[0x1dd40 + 0x1f];
    }
    rt %= 12;
    if (rt < 0)
        rt += 12;
    *root = rt;
    if (sc < 0 || sc > 34)
        return 0xfff;
    for (i = 0; i < 8; i++) {
        int n = (int)SCALE_TAB[8 * sc + i];
        if (n < 0)
            break;
        m |= 1 << (n % 12);
    }
    return m ? m : 0xfff;
}

/* the notes the loop plays: from the lowest note held (lo) up to below hi, those of the scale */
struct tm_pool {
    int lo, hi;
    int cls, root;      /* the scale */
    int n;              /* how many */
};

static int in_scale(const struct tm_pool *p, int x)
{
    return (p->cls >> ((x + 12 - p->root) % 12)) & 1;
}

static void tm_pool(struct tm_pool *p, int t, int lo, int range)
{
    int x;

    p->cls = tm_scale(t, &p->root);
    p->lo = lo;
    p->hi = lo + 12 * (clamp_range(range) + 1);
    if (p->hi > 128)
        p->hi = 128;
    p->n = 0;
    for (x = lo; x < p->hi; x++)
        p->n += in_scale(p, x);
}

/* the k-th note of the pool, lowest first (lo when the pool is empty: lo out of the scale near the top) */
static int tm_pick(const struct tm_pool *p, int k)
{
    int x;

    for (x = p->lo; x < p->hi; x++)
        if (in_scale(p, x) && k-- == 0)
            return x;
    return p->lo;
}

/* the place in the pool of a value (0..255) */
static int tm_place(int v, int n)
{
    return (int)(((u32)v * (u32)n) >> 8);
}

/* an on step in TM -> the step's note, or -1 (none held). With the chance LOOP CHANGE / 100 the step first takes
 * another note of the pool (its value then the middle of that note's share of 0..255); no other step changes. The
 * octave above the lowest note held goes to st + 0x1c, the note less those octaves to + 0x0c. */
int am_tm(int t, u8 *st, const u8 *sound, int mode)
{
    struct am_tm *s = &am_tml[t & 15];
    int lo = held_note(HELD + 4 * (t & 15), 0), i = (int)(*(volatile u32 *)st & 15), c = mode - 7, k, x;
    struct tm_pool p;

    tm_seed(s);
    if (lo < 0)
        return -1;
    tm_pool(&p, t & 15, lo, (signed char)sound[0x161]);
    k = tm_place(s->v[i], p.n);
    if (p.n > 1 && (c >= 100 || (c > 0 && (int)(rnd() % 100u) < c))) {
        k = (k + 1 + (int)(rnd() % (u32)(p.n - 1))) % p.n;
        s->v[i] = (u8)((256 * k + 128) / p.n);
    }
    x = tm_pick(&p, k);
    *(volatile u32 *)(st + 0x1c) = (u32)((x - lo) / 12);
    *(volatile u32 *)(st + 0x0c) = (u32)(x - 12 * ((x - lo) / 12));
    return x;
}

/* ---- the ARP menu */

static int ui_track(void)
{
    return TRACK_OF(P_TRACKS(PROJECT())) & 15;
}

static void *ui_sound(int t)
{
    return SOUND_OF(P_SOUNDS(PROJECT()), t);
}

static int mode_of(void *snd)
{
    return (signed char)GET_MODE(snd);
}

/* the menu's MODE name (jsr from 0x400188de, for getMode): TM's for TM, CYCL's for what plays as CYCL */
int am_mode_name(void *snd)
{
    int m = mode_of(snd);

    if (is_tm(m))
        return 7;
    if (m < 0 || m > 7)
        return 4;
    return m;
}

/* knob A (jsr from 0x40018eee, for setMode(snd, MODE + the turn)): OFF .. RAND, TM, holding at TM */
void am_knob_mode(void *snd, int v)
{
    int cur = mode_of(snd), t = ui_track(), at, to;
    int d = (signed char)(v - cur);                     /* the turn (the stock adds it to MODE in 8 bits) */

    at = is_tm(cur) ? 7 : cur < 0 || cur > 7 ? 4 : cur;
    to = at + d;
    if (to < 0)
        to = 0;
    if (to > 7)
        to = 7;
    if (is_tm(cur)) {
        if (to == 7)
            return;
        am_chg[t] = (u8)(cur - 7);
    }
    if (to == 7) {
        if (am_chg[t] > 100)
            am_chg[t] = 0;
        tm_seed(&am_tml[t]);
        SET_MODE(snd, 7 + am_chg[t]);
    } else
        SET_MODE(snd, to);
}

/* knob E (jsr from 0x400190ae, for setOffset(snd, step, offset + the turn)): in TM, LOOP CHANGE 0..100 */
void am_knob_e(void *snd, int step, int v)
{
    int cur = mode_of(snd), c;

    if (!is_tm(cur)) {
        SET_OFS(snd, step, v);
        return;
    }
    c = cur - 7 + (v - GET_OFS(snd, step));
    if (c < 0)
        c = 0;
    if (c > 100)
        c = 100;
    am_chg[ui_track()] = (u8)c;
    if (c != cur - 7)
        SET_MODE(snd, 7 + c);
}

/* the loop's bars, 1..21 for each step: the step's note's place in the pool (or its value with no note held) */
static void tm_bars(int t, void *snd, int len, u8 *h)
{
    struct am_tm *s = &am_tml[t];
    int lo = held_note(HELD + 4 * t, 0), n = 0, i;
    struct tm_pool p;

    if (lo >= 0) {
        tm_pool(&p, t, lo, (signed char)GET_RANGE(snd));
        n = p.n;
    }
    for (i = 0; i < len; i++) {
        if (n > 1)
            h[i] = (u8)(1 + tm_place(s->v[i], n) * 20 / (n - 1));
        else if (lo >= 0)
            h[i] = 1;
        else
            h[i] = (u8)(1 + s->v[i] * 20 / 255);
    }
}

static u32 mix(u32 x, u32 y)
{
    return (x << 5 | x >> 27) ^ y;
}

/* what the graph shows: the loop, the notes and scale it is put on, LEN, the step playing */
static u32 tm_sig(int t, void *snd)
{
    struct am_tm *s = &am_tml[t];
    int root, i;
    u32 x = mix((u32)tm_scale(t, &root), (u32)root);

    for (i = 0; i < 16; i++)
        x = mix(x, s->v[i]);
    x = mix(x, (u32)held_note(HELD + 4 * t, 0));
    x = mix(x, (u32)(u8)GET_LENM1(snd));
    x = mix(x, (u32)(u8)GET_RANGE(snd));
    x = mix(x, (u32)(u8)GET_MODE(snd));
    x = mix(x, HEADS[t] ? *(volatile u32 *)(ARPST + 40 * t) : 0xffu);
    return x | 1u;
}

/* ArpSetupMenuView slot 4: the stock menu, then in TM LOOP CHANGE on knob E and the loop in the step graph */
void am_menu_draw(void *view, void *bm)
{
    int t, m, len, i;
    void *snd, *font;
    u8 h[16];

    MENU_DRAW(view, bm);
    t = ui_track();
    snd = ui_sound(t);
    m = mode_of(snd);
    if (!is_tm(m)) {
        am_drawn = 0;
        return;
    }
    len = tm_len(GET_LENM1(snd));
    FILL(bm, 0, 8, 0x1b, 0x21, 0);                      /* knob E's column, above its (E) */
    font = FONT(SMALL);
    TEXT(bm, font, 0xf, 0x19, 1, 0, am_s_loop, am_s_loop);
    TEXT(bm, font, 0xf, 0x13, 1, 0, am_s_change, am_s_change);
    TEXT(bm, font, 0xf, 9, 1, 0, T_XXX, T_D, m - 7);
    FILL(bm, 0x1e, 4, 0x63, 0x1f, 0);                   /* the step graph, above the row of squares */
    tm_bars(t, snd, len, h);
    for (i = 0; i < len; i++) {
        int x = 0x21 + 4 * i;
        if ((u8)STEP_ON(snd, i))
            FILL(bm, x, 7, x + 2, 8 + h[i], 1);         /* 3 px at least */
        else
            FILL(bm, x + 1, 7, x + 1, 7, 1);            /* a rest: one dot */
    }
    if (HEADS[t]) {                                     /* the arp is playing: its step */
        int p = (int)*(volatile u32 *)(ARPST + 40 * t);
        if (p >= 0 && p < len)
            HLINE(bm, 0x21 + 4 * p, 5, 0x23 + 4 * p, 1);
    }
    DOTS_V(bm, 0x21 + 4 * len, 7, 0x1f, 1);             /* the loop's end, as the stock marks LEN */
    am_drawn = tm_sig(t, snd);
}

/* ArpSetupMenuView slot 11, the menu's tick: the stock tick, then a redraw in TM when the loop has moved */
void am_menu_tick(void *view)
{
    int t;
    void *snd;

    MENU_TICK(view);
    t = ui_track();
    snd = ui_sound(t);
    if (is_tm(mode_of(snd)) && tm_sig(t, snd) != am_drawn)
        INVALIDATE(view);
}
