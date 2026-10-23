/* SPDX-License-Identifier: GPL-2.0-or-later */
/* flanger-sync (Digitone II OS 1.12): FUNC + turn on the flanger's SPD knob (the chorus page's knob B, descriptor row
 * 0x6a, k 26) steps through speeds in sync with the tempo, as FUNC + turn on the delay's TIME steps through note
 * values. For dn2-cfw's FL02 build (github.com/naturarum/dn2-cfw, notes/T5.6-flanger-retune.md section 7), whose DSP
 * plays these values at the project tempo (tools/dspsplice/src/flg_glue3.c and flg_sync.c). EXPERIMENT:
 * emulator-tested only.
 *
 * How the stock OS does it: FUNC + turn on a parameter page reaches ParameterSet::vslot11 (0x4003780e) with FUNC
 * held, which asks the row's display entry for the new value: its std::function at +0x34, called as
 * (functor, value, delta, min, max, default) -> value. Most rows have the stock one that jumps to the minimum or the
 * maximum (0x400bbd20; the stock SPD too); the delay's TIME (row 0x71) has one that steps through the 14 note values
 * its value text names (0x400cb992). The display table is built at boot by the static initializer 0x400c2c4e, which
 * copies each row's +0x34 from a template; for row 0x6a, 0x400c5c96 `pea 0x42432ee8` (the shared template),
 * 0x400c5c9c `pea 0x42435eb8` (row 0x6a's +0x34), `jsr (a4)` (std::function's copy constructor 0x401adfb2).
 *
 * A synced speed lives in the knob's own value, as a low byte (the delay's fine values have one too):
 *     S(i) = (a_i << 8) | i,  i = 1..12
 * i is the flanger's FL_SYNC (one LFO cycle per 16, 8, 4, 2, 1 bars, 1/2, 1/4., 1/4, 1/8., 1/8, 1/16, 1/32 note) and
 * a_i the free SPD number closest to that speed at 120 BPM (the knob's glyph points there; a DSP without sync plays
 * about that speed). Every other value is a free speed, as on FL01. A kit stores the knob's 16-bit value as it is
 * (the FX record's serializer and loader copy the words), so a synced speed is saved and loaded with the kit; the
 * values the stock UI, MIDI and earlier builds make are whole numbers (low byte 0), never one of these.
 *
 * Two sites, made and checked against the stock image by dn2-cfw's scripts/fl02_make_mod.py:
 *  - row 0x6a +0x34, the value text: fls_fmt names the 12 synced values ("1/4" ...) and gives every other value to
 *    the stock plain-number text 0x400e3030;
 *  - 0x400c5c96 `pea 0x42432ee8` (keep2) -> fls_tpl: row 0x6a's +0x34 is copied from fls_tpl instead, so it is the
 *    stock template's manager (0x400bfbec: the empty functor allocated on copy) with fls_func as its function.
 * fls_func: from a synced value one step faster (turn right) or slower (left), held at the ends (as the delay at
 * 1/128 and 1/1); from a free value, the first synced value above it (right) or the last one below it (left), so the
 * first FUNC + turn lands near the speed the knob had. A turn without FUNC steps from the number shown, as before,
 * and so leaves the sync (the low byte stays: a free speed 0.05 of a step or less above the number shown); back on
 * the same number it is the synced speed again.
 */
typedef unsigned long u32;

#define SPRINTF    ((int (*)(char *, const char *, ...))0x40000e82)
#define FMT_NUMBER ((int (*)(int, char *))0x400e3030)   /* the stock plain number (SPD's own text) */
#define FN_MANAGER ((const void *)0x400bfbec)          /* the shared template's std::function manager */

#define S(a, i) (((a) << 8) | (i))
#define NSYNC 12

/* the synced values by FL_SYNC index (0: none); read by dn2-cfw's tools and checked against flg_sync.c */
const short fls_val[NSYNC + 1] __attribute__((aligned(4))) = {
    0, S(9, 1), S(23, 2), S(37, 3), S(52, 4), S(66, 5), S(80, 6), S(86, 7), S(94, 8), S(100, 9), S(108, 10),
    S(122, 11), S(126, 12)
};
static const char t1[] = "16/1", t2[] = "8/1", t3[] = "4/1", t4[] = "2/1", t5[] = "1/1", t6[] = "1/2", t7[] = "1/4.",
                  t8[] = "1/4", t9[] = "1/8.", t10[] = "1/8", t11[] = "1/16", t12[] = "1/32";
static const char *const fls_txt[NSYNC + 1] __attribute__((aligned(4))) = {
    0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12
};

/* value (<< 8, with its low byte) -> its FL_SYNC index 1..12, or 0 for a free speed */
static int fls_index(int value)
{
    int i = value & 0xff;
    if (i >= 1 && i <= NSYNC && value == fls_val[i])
        return i;
    return 0;
}

/* the value text, called as the stock ones: (value, out) -> what sprintf returns */
int fls_fmt(int value, char *out)
{
    int i = fls_index(value);
    if (i)
        return SPRINTF(out, "%s", fls_txt[i]);
    return FMT_NUMBER(value, out);
}

/* FUNC + turn: (functor, value, delta, min, max, default) -> the new value (the caller clamps it to min..max) */
int fls_func(void *functor, int value, int delta, int min, int max, int def)
{
    int i = fls_index(value);
    (void)functor;
    (void)min;
    (void)max;
    (void)def;
    if (i) {
        i += delta >= 0 ? 1 : -1;
        if (i < 1)
            i = 1;
        if (i > NSYNC)
            i = NSYNC;
    } else if (delta >= 0) {
        for (i = 1; i < NSYNC && fls_val[i] <= value; i++)
            ;
    } else {
        for (i = NSYNC; i > 1 && fls_val[i] >= value; i--)
            ;
    }
    return fls_val[i];
}

/* the std::function row 0x6a's +0x34 is copied from: functor, manager, invoker (libstdc++'s layout: +0 the functor's
 * 8 bytes, +8 the manager, +0xc the invoker; 0x401adfb2 copies +8 and +0xc and has the manager clone +0) */
struct fn {
    const void *functor;
    u32 pad;
    const void *manager;
    int (*invoker)(void *, int, int, int, int, int);
};
static const char fls_byte;
const struct fn fls_tpl __attribute__((aligned(4))) = { &fls_byte, 0, FN_MANAGER, fls_func };
