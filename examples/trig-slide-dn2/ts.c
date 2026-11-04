/* SPDX-License-Identifier: GPL-2.0-or-later */
/* trig-slide (Digitone II OS 1.12): parameter slides between trigs, like portamento for p-locks. With SLIDE on (a new
 * knob, E on the TRIG page's second page, RTRG, next to PTIM and PORT), when a note's gate goes off the track's
 * parameters glide in a straight line to the values its next trig sets (the sound, with that trig's p-locks), arriving
 * as the next trig plays. No gap (LEN reaching the next trig, INF, micro-timing making them overlap): no slide, the
 * values change at the trig as before. For dn2-cfw's TS01 build (github.com/naturarum/dn2-cfw,
 * notes/T2.9-trig-slides.md). EXPERIMENT: not yet run on a unit.
 *
 * How the stock does it, and where this hooks in:
 *  - The sequencer task 0x400d99de builds each step's event (0x400d87b8: [4] the track, [2] its time less its
 *    micro-timing, [0xe] its trig flags, [0xf] the sound's parameter set, [0x15] its lock set: +8 count, then (k,
 *    value) in 8-byte entries from +0x14) a step before it plays and queues it with 0x40138e3c(event, time)
 *    (0x400da184). ts_queue takes that call (through ts_queue_hook, ts_stubs.s), and ts_start play start's for each
 *    track's first trig (built before play by 0x400d907a; 0x400d7e88, 0x400d806a): each publishes the trig (its time
 *    and values), queues it, and looks ahead for the trig after it, asking the same builder about the following steps
 *    with an event of its own, as the stock's 0x400d907a does (a supplied event: no condition counters move); it puts
 *    back the random generator and the preview's PRE state the builder moves, so nothing that plays changes.
 *  - The block handler 0x40025e36 (1,500 a second) ends a voice's gate with 0x4002b06e(v), through five register
 *    loads of its address (keep2 sites): ts_gateoff starts the track's slide there, from the voice's row of the
 *    parameter mirror (0x80003b12 + 0xca r + 2k), toward the earliest trig still to play, if no other voice of the
 *    track is held (their records name it) and the track is playing.
 *  - The handler takes every event due before clock + 2 rate (the clock 0x80005398 moves 2 rate a block), copies its
 *    set into the voice's row (0x4002549c) and writes its locks into the row and straight into the smoothing state
 *    (0x400db1f6); then it smooths every row (0x400db28e, 0x4002717e). ts_block takes that call: it moves every row the
 *    track owns (0x80005308 + 4r), row word and smoothing state, so the slide is exactly linear, writes the target in
 *    full the block before the trig is taken and stops, so the trig has nothing left to change; it stops at once when
 *    a note is held on the track (the trig came, or another note), and returns the smoothing's value.
 * What slides: a parameter of the track's machine and filter (0x400dc18e(k, machine, filter) -> its row) whose range
 * spans 50 values or more, but LFO DEST and portamento; lists, switches and modes still change at the trig. A next
 * trig with another machine or filter (a sound lock): no slide. A target the trig does not lock follows the sound, so
 * a knob turned during a slide moves where it lands. The mirror's words are signed (the smoothing reads them so).
 *
 * SLIDE is the sound's word k 100 (0 off, 1 << 8 on), free in every machine (notes/T6.1 3.2, which keeps 65 and 100
 * off persistent rows because the stock storage does not keep them): the stock sound serializer 0x400dd80a copies
 * k 0..99 through its map and clears stored ids 0..107, the loader 0x400dd34e reads ids 0..106, so stored id 107 is
 * free; ts_stubs.s keeps k 100 there (a stock OS drops it: SLIDE off). Per step (0.2): SLIDE is p-lockable; a lock
 * on a trig says whether the slide into that trig happens, a trig without one follows its sound's SLIDE. The
 * pattern's locks are stored by stored id too: the lock serializer 0x400de87c maps k through the same map (k 100
 * would read past its end, into the reverse map: id 0), so ts_lmap gives it a copy with k 100 -> 107, and the lock
 * loader 0x400de682, which keeps ids 0..106 only (a stock OS drops the lane), takes 107 back as k 100 (ts_lpre,
 * ts_lconv). Row 0x155 through param-grow, Overbridge id 0x420, no MIDI CC, no randomiser, no LFO destination.
 */
typedef unsigned int u32;           /* 32 bits, on the unit and on a host test */
typedef int s32;
typedef unsigned short u16;
typedef short s16;
typedef unsigned char u8;
typedef signed char s8;

#define NT      16                  /* tracks */
#define NROW    16                  /* voices: rows of the parameter mirror */
#define NK      100                 /* sound words that may slide: k 1..99 */
#define K_SLIDE 100
#define ID_SLIDE 107                /* its stored id, in the kit and in the pattern's locks */
#define NLOCK   101                 /* a step's locks: one per sound word at most */
#define TICK    900000u             /* the sequencer's tick, in the clock's units */
#define FULL    (1 << 15)           /* progress, Q15 */

#ifndef TS_HOST                      /* the stock OS's (a host test supplies its own: dn2-cfw scripts/ts01/test_ts.c) */
#include "../param-grow-dn2/param_rows.h"

#define M32(a)    (*(volatile u32 *)(unsigned long)(a))
#define MS16(a)   (*(volatile s16 *)(unsigned long)(a))
#define M8(a)     (*(volatile u8 *)(unsigned long)(a))
#define MS8(a)    (*(volatile s8 *)(unsigned long)(a))
#define ADDR(p)   ((u32)(unsigned long)(p))

#define Q_INSERT  ((void (*)(u32, u32))0x40138e3c)          /* queue an event at a time */
#define EV_BUILD  ((u32 (*)(s32, u32, u32, s32, u32, s32 *, s32 *))0x400d87b8)   /* (track, pattern, kit, step,
                                                                                    event, &micro, &swing) */
#define GATE_OFF  ((void (*)(u32))0x4002b06e)
#define SMOOTH    ((u32 (*)(u32))0x400db28e)               /* returns its output in d0; the handler passes it on */
#define ROW_OF    ((u32 (*)(u32, s32, s32))0x400dc18e)      /* (k, machine, filter) -> descriptor row */
#define SWING     ((s32 (*)(void))0x400d7e34)              /* the swing amount, as the builder reads it */
#define CONV      ((u32 (*)(u32, u32))0x400dce24)         /* (track, stored id) -> k, for the lock loader */

#define STR_PORTAMENTO ((const char *)0x4021095f)   /* "Portamento": PTIM's and PORT's group name */
#define FMT_ONOFF      ((const void *)0x400e3568)   /* the stock ON / OFF text */
#define LOOK_RSET      0x32                         /* Env. Reset: an ON/OFF sound row's display entry */

/* the knob: row 0x155, the sound's k 100, 0..1, on page 2 (RTRG) slot E; p-lockable (flags as Env. Reset's) */
const struct param_row ts_rows[] __attribute__((aligned(4))) = {
    { 0x155, { 0x0e, K_SLIDE, V(0), V(1), V(0), 0, -1, -1, -1, 0x420, 0,
               "Param Slide", STR_PORTAMENTO, "SLD", FMT_ONOFF, STR_EMPTY }, LOOK_RSET, 0, 0, 2, 4 },
    { 0 }
};

static u32 ts_ev_buf[0x6c / 4];         /* the look-ahead's event (the stock's are 0x6c bytes) */
static u32 ts_lk_buf[(0x14 + 8 * NLOCK + 3) / 4];   /* and its lock set */
#define TS_EV ADDR(ts_ev_buf)
#define TS_LK ADDR(ts_lk_buf)
#endif

/* the stock OS's data (a host test maps these addresses into memory of its own) */
#define DESC      0x401f83a4u              /* the stock descriptor table (rows 0..0x140) */
#define MAP       0x401fd330u              /* k -> stored id, a word each, k 0..99 (0x400dd80a, 0x400de87c) */
#define CLOCK     M32(0x80005398)          /* the audio clock, the events' time units */
#define RATE      M32(0x402a132c)          /* the clock moves 2 RATE a block */
#define KIT       M32(0x800052a0)          /* the live kit (the handler's) */
#define OWNER(r)  M32(0x80005308 + 4 * (r))
#define RAW(r, k) MS16(0x80003b12 + 0xca * (r) + 2 * (k))
#define ROW_MACH(r) MS8(0x8000487c + 0x99 * (r))   /* the machine and filter of the set last copied in (0x4002549c) */
#define ROW_FILT(r) MS8(0x8000487d + 0x99 * (r))
#define STATE(r, k) M32(0x8000de60 + 4 * (0x11 + 0x65 * (r) + (k)))
#define VTRACK(v) MS8(0x40288306 + 0x14 * (v))
#define VNOTE(v)  MS8(0x40288307 + 0x14 * (v))
#define PLAYING(t) M32(0x42c5b924 + 4 * (t))   /* 1: the sequencer plays the track */
#define SEQ_STEP(t) M8(0x42c5b964 + (t))       /* the step the scheduler builds */
#define START_STEP(t) M8(0x42c5ba73 + (t))     /* the step 0x400d907a built a track's first event for */
#define SEQ_PAT   M32(0x42c5b9a0)
#define SEQ_KIT   M32(0x42c5b9ac)
#define SEQ_KIT2  M32(0x42c5b9b4)
#define SEQ_TSPEED(t) MS8(0x42c5b9d9 + (t))     /* the speed the track's tick counter runs at */
#define TICKS(sp) ((s32)M32(0x401f6f18 + 4 * (sp)))
#define RNG0      M32(0x402a12fc)          /* the sequencer's random generator (0x40136984) */
#define RNG1      M32(0x402a1300)
#define PRE_PREVIEW(t) M8(0x4463fc79 + (t))   /* the PRE result a supplied event's build leaves (0x401369cc) */

#define SOUND(kit, t) ((kit) + 0x34 + 0x48b * (u32)(t))
#define LANE(pat, t, k) MS8((pat) + 0x17e50 + 0x50a0 + 0x65 * (u32)(t) + (k))   /* the track's lock lane for k, or -1 */
#define SWORD(set, k) MS16((set) + 0x14 + 2 * (k))

/* a trig still to play on a track: published by the sequencer task, read by the block handler (two copies, the
   index flipped once a copy is whole, then the generation: the handler always reads a whole one) */
struct ts_rec {
    u32 time;                       /* when it plays, on the audio clock */
    s32 step;                       /* the step it plays on */
    u32 set;                        /* the parameter set it copies; 0: none */
    u32 n;                          /* its locks */
    s16 slide;                      /* its SLIDE lock, or -1: its sound's */
    s16 lk[NLOCK];
    s16 lv[NLOCK];
};
struct ts_pub {
    volatile u32 ix;
    volatile u32 gen;
    struct ts_rec r[2];
};
struct ts_move {
    u8 k;
    u8 locked;                      /* the trig locks it: v1 fixed; else v1 follows the sound */
    s16 v0, v1;
};
struct ts_slide {
    u32 on;
    u32 t0, t1;                     /* its start and its arrival, on the audio clock */
    s32 step;                       /* the trig it moves toward */
    u32 gen;                        /* ts_exact's generation it last read */
    u32 set;                        /* that trig's set */
    u32 n;
    u32 bylock;                     /* its trig's SLIDE lock asked for it (else the sound's SLIDE: off stops it) */
    struct ts_move m[NK];
};

static struct ts_pub ts_exact[NT];      /* the trig just queued */
static struct ts_pub ts_next[NT];       /* the trig after it, looked ahead */
static struct ts_slide ts_sl[NT];
static u32 ts_mask[8][8][4];            /* per machine and filter, bit k: k slides; bit 0: worked out */
static u32 ts_active;                   /* bit t: ts_sl[t].on (one read a block when nothing slides) */

static int ts_on(u32 kit, s32 t)
{
    return kit && SWORD(SOUND(kit, t), K_SLIDE) >= 0x80;
}

/* the track's held voices: those whose record names it (0x4002b06e sets track and note to -1 when a gate ends) */
static u32 ts_held(s32 t)
{
    u32 v, m = 0;
    for (v = 0; v < NROW; v++)
        if (VTRACK(v) == t && VNOTE(v) >= 0)
            m |= 1u << v;
    return m;
}

static void ts_publish(struct ts_pub *p, u32 time, s32 step, u32 set, u32 locks)
{
    u32 w = p->ix ^ 1, i, n = locks ? M32(locks + 8) : 0;
    struct ts_rec *r = &p->r[w];
    if (n > NLOCK)
        n = NLOCK;
    r->time = time;
    r->step = step;
    r->set = set;
    r->slide = -1;
    for (i = 0; i < n; i++) {
        r->lk[i] = MS16(locks + 0x14 + 8 * i);
        r->lv[i] = MS16(locks + 0x16 + 8 * i);
        if (r->lk[i] == K_SLIDE)
            r->slide = r->lv[i];
    }
    r->n = n;
    __asm__ __volatile__("" ::: "memory");     /* the copy whole before the handler can see it */
    p->ix = w;
    p->gen = p->gen + 1;
}

/* the set an event copies (as the handler picks it) */
static u32 ts_set(u32 e, u32 kit, s32 t)
{
    if (M32(e + 0x40))
        return M32(e + 0x40);
    if (M32(e + 0x3c))
        return M32(e + 0x3c);
    return SOUND(kit, t);
}

/* ticks per step as the builder reads them, the table word's low byte (its swing is in these) */
static s32 ts_btps(u32 pat, s32 t)
{
    s32 sp = MS8(pat + 0x1dd59) == 1 ? MS8(pat + 0x6a5 * (u32)t + 0x48f) : MS8(pat + 0x1dd5a);
    return (s8)TICKS(sp);
}

/* a trig event on its way into the queue, for its step: published, queued, and the trig after it looked ahead */
static void ts_take(u32 ev, u32 time, s32 t, s32 step)
{
    u32 pat, kit, base, trk;
    s32 len, tps, k, scan;
    /* everything from the event before it is queued: the handler may take it and free it at once */
    pat = M32(ev + 0x1c) - 0x6a50;
    kit = pat == SEQ_PAT ? SEQ_KIT : SEQ_KIT2;
    ts_publish(&ts_exact[t], time, step, ts_set(ev, kit, t), M32(ev + 0x54));
    base = M32(ev + 8);                                     /* its time less its micro-timing */
    if (M32(ev + 0x38) & 0x10)
        base -= (u32)(SWING() * ts_btps(pat, t) * 18000);   /* and less its swing: its step's place */
    Q_INSERT(ev, time);

    /* the trig after it: the following steps (the step counter's length and speed). The builder makes an event only
       for a step whose flags (pattern + 0x6a5 t + 2 step) have bit 0 set and bit 2 clear: those are read first, one
       read of DDR a step instead of a whole build (this OS reads DDR uncached), unless the track remaps its steps
       (+0x5a4) or plays in its other mode (+0x498 == 1), where the builder reads other flags: then every step */
    trk = pat + 0x6a5 * (u32)t;
    scan = !M8(trk + 0x5a4) && MS8(trk + 0x498) != 1;
    len = MS8(pat + 0x1dd59) ? MS16(trk + 0x48c) : MS16(pat + 0x1dd54);
    tps = TICKS(SEQ_TSPEED(t));
    if (len >= 1 && len <= 256 && tps >= 1 && tps <= 1000)    /* tps TICK fits: the span stops before it wraps */
        for (k = 1; k <= len; k++) {
            u32 span = (u32)k * (u32)tps * TICK, r, s0, s1, i;
            s32 micro = 0, swing = 0, s = (step + k) % len;
            u8 pre;
            if (span > 0x40000000u)
                break;
            if (scan && (MS16(trk + 2 * (u32)s) & 5) != 1)
                continue;
            for (i = 0; i < 0x6c / 4; i++)
                M32(TS_EV + 4 * i) = 0;
            M32(TS_EV + 0x54) = TS_LK;
            M32(TS_LK + 8) = 0;
            s0 = RNG0;
            s1 = RNG1;
            pre = PRE_PREVIEW(t);
            r = EV_BUILD(t, pat, kit, s, TS_EV, &micro, &swing);
            RNG0 = s0;
            RNG1 = s1;
            PRE_PREVIEW(t) = pre;
            if (r) {
                s32 m = (s32)(s16)((M32(r + 0xc) * (u32)micro) >> 16) * 1500;   /* as the scheduler scales it */
                ts_publish(&ts_next[t], base + span + (u32)swing + (u32)m, s, ts_set(r, kit, t), M32(r + 0x54));
                return;
            }
        }
    ts_publish(&ts_next[t], 0, -1, 0, 0);
}

/* the event's track if trig-slide follows it: a sound track whose SLIDE is on or locked on some step */
static int ts_track(u32 ev)
{
    s32 t = (s32)M32(ev + 0x10);
    if (t < 0 || t >= NT || ((MS16(SEQ_KIT + 0x5cda) >> t) & 1))                 /* a MIDI track */
        return -1;
    if (!ts_on(KIT, t) && LANE(M32(ev + 0x1c) - 0x6a50, t, K_SLIDE) < 0)
        return -1;
    return t;
}

/* in place of the sequencer task's 0x40138e3c(event, time) for a step's trig event (0x400da184; ts_queue_hook) */
void ts_queue(u32 ev, u32 time)
{
    s32 t = ts_track(ev);
    if (t < 0)
        Q_INSERT(ev, time);
    else
        ts_take(ev, time, t, SEQ_STEP(t));
}

/* in place of play start's 0x40138e3c(event, time) for a track's first trig, built before play (0x400d7e88 and
   0x400d806a load the call's address into d7 for their loop over the tracks; ts_start_hook) */
void ts_start(u32 ev, u32 time)
{
    s32 t = ts_track(ev);
    if (t < 0)
        Q_INSERT(ev, time);
    else
        ts_take(ev, time, t, START_STEP(t));
}

/* does k slide, for this machine and filter? */
static int ts_slides(u32 k, s32 m, s32 f)
{
    u32 row, d;
    if (k == 0 || k == 4 || k == 12 || k == 20 || k == 93 || k == 94 || k >= NK)
        return 0;                   /* LFO DEST (a list), portamento */
    row = ROW_OF(k, m, f);
    if (row == 0 || row > 0x140)
        return 0;
    d = DESC + 0x3c * row;
    return (s32)M32(d + 0xc) - (s32)M32(d + 8) >= (50 << 8);
}

static const u32 *ts_masks(s32 m, s32 f)
{
    u32 *mk, k;
    if ((u32)m >= 8 || (u32)f >= 8)
        return 0;
    mk = ts_mask[m][f];
    if (!(mk[0] & 1)) {
        for (k = 1; k < NK; k++)
            if (ts_slides(k, m, f))
                mk[k >> 5] |= 1u << (k & 31);
        mk[0] |= 1;
    }
    return mk;
}

/* a slide toward r, from the values in row src; 0 (and off) if nothing would move */
static int ts_build(struct ts_slide *sl, const struct ts_rec *r, u32 src, u32 now)
{
    u32 k, i, n = 0, m0, m1, m2, m3, lk[4];
    s32 m, f;
    const u32 *mk;
    sl->on = 0;
    if (!r->set)
        return 0;
    m = MS8(r->set + 0xde);
    f = MS8(r->set + 0xdf);
    if (m != ROW_MACH(src) || f != ROW_FILT(src) || !(mk = ts_masks(m, f)))
        return 0;                   /* the row holds another machine or filter: the trig changes it */
    m0 = mk[0], m1 = mk[1], m2 = mk[2], m3 = mk[3];
    lk[0] = lk[1] = lk[2] = lk[3] = 0;
    for (i = 0; i < r->n; i++) {            /* which k the trig locks */
        k = (u16)r->lk[i];
        if (k < NK)
            lk[k >> 5] |= 1u << (k & 31);
    }
    for (k = 1; k < NK; k++) {
        u32 w = k < 32 ? m0 : k < 64 ? m1 : k < 96 ? m2 : m3;
        s16 a, b;
        u8 locked;
        if (!((w >> (k & 31)) & 1))
            continue;
        a = RAW(src, k);
        locked = (lk[k >> 5] >> (k & 31)) & 1;
        if (locked) {
            for (i = 0; (u32)(u16)r->lk[i] != k; i++)
                ;
            b = r->lv[i];
        } else
            b = SWORD(r->set, k);
        if (a == b)
            continue;
        sl->m[n].k = (u8)k;
        sl->m[n].locked = locked;
        sl->m[n].v0 = a;
        sl->m[n].v1 = b;
        n++;
    }
    if (!n)
        return 0;
    sl->n = n;
    sl->bylock = r->slide >= 0;
    sl->set = r->set;
    sl->t0 = now;
    sl->t1 = r->time;
    sl->step = r->step;
    sl->on = 1;
    return 1;
}

/* does the slide into this trig happen: its SLIDE lock, else its sound's SLIDE */
static int ts_wants(const struct ts_rec *r)
{
    return (r->slide >= 0 ? r->slide : SWORD(r->set, K_SLIDE)) >= 0x80;
}

/* the earliest trig still to play: the one queued, else the one looked ahead */
static const struct ts_rec *ts_upcoming(s32 t, u32 now)
{
    const struct ts_rec *a = &ts_exact[t].r[ts_exact[t].ix], *b = &ts_next[t].r[ts_next[t].ix];
    if (a->set && (s32)(a->time - now) > 0)
        return a;
    if (b->set && (s32)(b->time - now) > 0)
        return b;
    return 0;
}

/* in place of 0x4002b06e(v), the handler's end of a voice's gate (five sites load its address) */
void ts_gateoff(u32 v)
{
    if (v < NROW) {
        s32 t = VTRACK(v);
        if (t >= 0 && t < NT && VNOTE(v) >= 0 && PLAYING(t) == 1
            && !(ts_held(t) & ~(1u << v))) {            /* the track's last held voice */
            u32 now = CLOCK;
            struct ts_slide *sl = &ts_sl[t];
            const struct ts_rec *r = ts_upcoming(t, now);
            sl->on = 0;
            if (r && ts_wants(r) && ts_build(sl, r, v, now))
                sl->gen = ts_exact[t].gen;
            if (sl->on)
                ts_active |= 1u << t;
            else
                ts_active &= ~(1u << t);
        }
    }
    GATE_OFF(v);
}

/* the slide's values at Q15 progress p, into every row the track owns, row word and smoothing state */
static void ts_write(struct ts_slide *sl, s32 t, s32 p)
{
    u32 r, i, rows = 0, n = sl->n, set = sl->set;
    for (r = 0; r < NROW; r++)              /* the owner words are in SRAM */
        if (OWNER(r) == (u32)t)
            rows |= 1u << r;
    for (i = 0; i < n; i++) {               /* each value once (the move list is in DDR), into every row */
        struct ts_move *m = &sl->m[i];
        u32 k = m->k;
        s32 a = m->v0, b, v;
        if (m->locked)
            b = m->v1;
        else
            m->v1 = (s16)(b = SWORD(set, k));
        v = a + (((b - a) * p) >> 15);
        for (r = 0; rows >> r; r++)
            if ((rows >> r) & 1) {
                RAW(r, k) = (s16)v;
                STATE(r, k) = (u32)v << 16;
            }
    }
}

static s32 ts_progress(u32 e, u32 d)
{
    while (d > 0x7fff) {
        d >>= 1;
        e >>= 1;
    }
    if (!d || e >= d)
        return FULL;
    return (s32)((e << 15) / d);
}

/* one slide one block on (sl->on cleared when it ends) */
static void ts_tick(struct ts_slide *sl, s32 t, u32 now, u32 due, u32 next)
{
    if (!sl->on || !sl->set)
        return;
    if (ts_held(t) || PLAYING(t) != 1 || (!sl->bylock && SWORD(sl->set, K_SLIDE) < 0x80)) {
        sl->on = 0;                 /* a note on the track (its trig came), stopped, or its sound's SLIDE off */
        return;
    }
    if (ts_exact[t].gen != sl->gen) {   /* a trig queued: the one it moves toward, or one that plays before */
        const struct ts_rec *r;
        sl->gen = ts_exact[t].gen;
        r = &ts_exact[t].r[ts_exact[t].ix];
        if (r->set && (s32)(r->time - now) > 0 && (r->step == sl->step || (s32)(r->time - sl->t1) < 0)) {
            u32 src;
            for (src = 0; src < NROW && OWNER(src) != (u32)t; src++)
                ;
            if (src == NROW || !ts_wants(r) || !ts_build(sl, r, src, now)) {
                sl->on = 0;
                return;
            }
        }
    }
    if ((s32)(sl->t1 - due) < 0) {          /* taken this block: the trig set its values */
        sl->on = 0;
        return;
    }
    if ((s32)(sl->t1 - next) < 0) {         /* taken next block: arrive now */
        ts_write(sl, t, FULL);
        sl->on = 0;
        return;
    }
    ts_write(sl, t, ts_progress(now - sl->t0, sl->t1 - sl->t0));
}

/* in place of the handler's 0x400db28e(mirror) (0x4002717e): every slide one block on, then the smoothing */
u32 ts_block(u32 mirror)
{
    u32 act = ts_active;
    s32 t;
    if (act) {
        u32 now = CLOCK, due = now + 2 * RATE, next = due + 2 * RATE;
        for (t = 0; act >> t; t++)
            if ((act >> t) & 1) {
                ts_tick(&ts_sl[t], t, now, due, next);
                if (!ts_sl[t].on || !ts_sl[t].set) {
                    ts_sl[t].on = 0;
                    act &= ~(1u << t);
                }
            }
        ts_active = act;
    }
    return SMOOTH(mirror);
}

/* the pattern's lock serializer 0x400de87c maps k to a stored id through MAP (a word each, k 0..99): ts_lmap
   (ts_stubs.s) gives it this copy instead, with SLIDE (k 100) -> 107; filled from MAP the first time */
u32 ts_map[K_SLIDE + 1];

u32 ts_map_fill(void)
{
    u32 k;
    if (ts_map[K_SLIDE] != ID_SLIDE) {
        for (k = 0; k < K_SLIDE; k++)
            ts_map[k] = M32(MAP + 4 * k);
        ts_map[K_SLIDE] = ID_SLIDE;
    }
    return ADDR(ts_map);
}

/* in place of 0x400dce24(track, stored id) -> k, which the pattern's lock loader 0x400de682 calls through a5
   (ts_lpre): stored id 107 on a sound track is SLIDE */
u32 ts_lconv(u32 t, u32 id)
{
    if (t < NT && id == ID_SLIDE)
        return K_SLIDE;
    return CONV(t, id);
}
