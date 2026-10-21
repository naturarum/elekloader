/* SPDX-License-Identifier: GPL-2.0-or-later */
/* comp-link-dn2 0.2 (Digitone II OS 1.12): the 3-band compressor's band values saved per kit.
 * EXPERIMENT: emulator-tested only. dn2-cfw (github.com/naturarum/dn2-cfw)
 * notes/T6.4-compressor-map-and-plan.md 3.1-3.3 (the design), notes/T6.3-cmp3.md (the build).
 *
 * Where: the FX record of a stored kit (fxSetupStorage_v0_t, 0xa0 bytes at stored kit +0x16ac):
 * +0 version (0), +4 + 2 id the 46 stock values, +0x98 routing, +0x9c zero, and +0x60..+0x97, 28
 * words that the stock serializer FUN_400dcf34 never writes and the stock loader FUN_400dcef0 never
 * reads (every kit saved so far holds zeros there). Layout 1:
 *     +0x60 + 2 (k - 70)   the live FX word of k 70..96 (value << 8), raw: the band rows k 70..86,
 *                          the band mixes k 87..89 (param-rows 0.3), the spare k 90..96
 *     +0x96                0xC3B1, the check word (the low byte is the layout)
 * A constant check word, not a checksum: the UI task's single edits (compl_update) and the storage
 * job's serializer can interleave, and any mix of the two is still a record of real values.
 *
 * Three sites (mod.json; store.s): the FX record serializer and loader (entry hooks: the stock body
 * first, then compl_encode / compl_decode on success), and FxSetup's vtable slot 17 (0x401da4f0,
 * FxSetup::updateMirror 0x4002ffca): every single change of an FX setup reaches the working image
 * through it (vslot04), so the band values follow a knob turn there as the stock values do.
 *
 * Load (compl_decode, after the stock loader succeeded: version 0; a record whose version is not 0
 * makes the stock loader return 0, and its caller FUN_400dddb6 then sets every FX word to its row's
 * default, FUN_400e7140, so nothing is decoded here):
 * - check word present: each word against its row's range; a word above it takes the row's
 *   default (the new-kit default of param-rows 0.3; dn2-cfw scripts/t63b_build.py checks this table
 *   against the rows);
 * - check word missing (every kit saved before CMP3, a kit saved by a stock OS, an export): seeded
 *   from the kit's own stock compressor values, which the stock loader has just loaded: each band
 *   takes THR, ATK, REL and MUP (k 60..63) and RAT = the stock ratio index (k 64, 0..7) + 1 (the band
 *   rows' 0 is 1.00; 1..8 are the stock 1.50..20.00), the Low/Mid crossover 0 (30 Hz), the Mid/High
 *   one 127 (16 kHz), each band's mix the stock MIX (k 67): the kit opens as one full-band compressor
 *   with its old settings. A seeded value out of its row's range takes the row's default. The spare
 *   k 90..96 load as 0.
 * Single edits (compl_update): the stock routine first; it writes a single FX change into the
 * record word of the change's id, and every k from 70 has id 0 (the map 0x401fd160 ends at k 69),
 * so a band knob turn writes the band value over the id-0 word. That word is put back when every k
 * the change names is 70 or more (a k below 70 is the stock behaviour, untouched), then the 27 words
 * and the check word are written from the live setup.
 */
typedef unsigned short u16;

#define K0 70
#define NV 27
#define MAGIC 0xC3B1u
#define REC(r, o) (*(volatile u16 *)((unsigned char *)(r) + (o)))

/* each word's row: the highest value (<< 8 gives the highest word) and the new-kit default.
   Order k 70..96: LOW THR RAT ATK REL MUP XOVR, MID the same, HIGH THR RAT ATK REL MUP, the three
   mixes, seven spare (0..127, default 0) */
const unsigned char compl_vmax[NV] = {
    127, 8, 127, 127, 127, 127,  127, 8, 127, 127, 127, 127,  127, 8, 127, 127, 127,
    127, 127, 127,  127, 127, 127, 127, 127, 127, 127,
};
const unsigned char compl_vdef[NV] = {
    32, 4, 24, 32, 64, 68,  32, 4, 24, 32, 64, 44,  32, 4, 24, 32, 64,
    0, 0, 0,  0, 0, 0, 0, 0, 0, 0,
};

void compl_encode(unsigned char *rec, const u16 *live)
{
    int i;
    for (i = 0; i < NV; i++)
        REC(rec, 0x60 + 2 * i) = live[K0 + i];
    REC(rec, 0x60 + 2 * NV) = MAGIC;
}

/* v, or the default of word i when v is above its row's range */
static u16 in_range(int i, unsigned int v)
{
    if (v > (unsigned int)compl_vmax[i] << 8)
        v = (unsigned int)compl_vdef[i] << 8;
    return (u16)v;
}

void compl_decode(u16 *live, const unsigned char *rec)
{
    volatile u16 *lv = live;
    int i, b;
    if (REC(rec, 0x60 + 2 * NV) == MAGIC) {
        for (i = 0; i < NV; i++)
            lv[K0 + i] = in_range(i, REC(rec, 0x60 + 2 * i));
        return;
    }
    /* seed: the stock k 60..64 and 67 as the stock loader just set them */
    {
        unsigned int thr = lv[60], atk = lv[61], rel = lv[62], mup = lv[63];
        unsigned int rat = (unsigned int)lv[64] + 0x100, mix = lv[67];
        for (b = 0; b < 3; b++) {
            i = 6 * b;
            lv[K0 + i + 0] = in_range(i + 0, thr);
            lv[K0 + i + 1] = in_range(i + 1, rat);
            lv[K0 + i + 2] = in_range(i + 2, atk);
            lv[K0 + i + 3] = in_range(i + 3, rel);
            lv[K0 + i + 4] = in_range(i + 4, mup);
            lv[K0 + 17 + b] = in_range(17 + b, mix);
        }
        lv[75] = 0;                     /* Low/Mid X-Over 30 Hz */
        lv[81] = 127 << 8;              /* Mid/High X-Over 16.0k */
        for (i = 20; i < NV; i++)
            lv[K0 + i] = 0;
    }
}

/* Stock 1.12: FxSetup::updateMirror, the C++ runtime's dynamic cast, the type infos it casts a
   change to (one k at +4; or a vector of k, begin +4, end +8) */
#define UPDATE   ((void (*)(void *, void *, void *))0x4002ffca)
#define DYNCAST  ((int *(*)(void *, const void *, const void *, int))0x401c9dba)
#define TI_SRC   ((const void *)0x401d90cc)
#define TI_MANY  ((const void *)0x401da16c)
#define TI_ONE   ((const void *)0x401da160)

/* 1 when every k the change names is K0 or more (the casts as updateMirror makes them) */
static int only_ours(void *info)
{
    int *p = DYNCAST(info, TI_SRC, TI_MANY, 0);
    if (p) {
        const int *k = (const int *)p[1], *e = (const int *)p[2];
        if (k == e)
            return 0;
        for (; k != e; k++)
            if (*k < K0)
                return 0;
        return 1;
    }
    p = DYNCAST(info, TI_SRC, TI_ONE, 0);
    return p && p[1] >= K0;
}

/* FxSetup's vtable slot 17 (0x401da4f0): (self, the record in the working image, the change or 0) */
void compl_update(void **self, unsigned char *stored, void *info)
{
    const u16 *live;
    u16 w0 = 0;
    if (stored)
        w0 = REC(stored, 4);
    UPDATE(self, stored, info);
    if (!stored)
        return;
    live = ((const u16 *(*)(void *))((void **)*self)[0x28 / 4])(self);
    if (!live)
        return;
    if (info && only_ours(info))
        REC(stored, 4) = w0;
    compl_encode(stored, live);
}
