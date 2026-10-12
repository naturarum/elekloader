/* SPDX-License-Identifier: GPL-2.0-or-later */
/* param_rows.h: the row a mod adds through param-grow's table param_rows (Digitone II
 * OS 1.12). grow.s reads these: keep the layout (0x54 bytes, offsets in the comments).
 * dn2-cfw (github.com/naturarum/dn2-cfw) notes/T6.5-table-growth.md. */
#ifndef PARAM_ROWS_H
#define PARAM_ROWS_H

/* The parameter descriptor, in the stock layout (0x3c bytes; min/max/default are << 8). */
struct param_desc {
    int group;                  /* +0x00 the page group (0x10 chorus, 0x11 reverb, 0x12 delay,
                                 *       0x13 master, 0x14 compressor, 0x15 ext. inputs, ...) */
    int k;                      /* +0x04 the slot it edits in its set (FX: 0..100) */
    int min, max, def;          /* +0x08 +0x0c +0x10 */
    int bipolar;                /* +0x14 */
    short cc, cc2;              /* +0x18 +0x1a MIDI CC (MSB, LSB), -1 for none */
    int nrpn;                   /* +0x1c -1 for none (and never above 398) */
    int uid;                    /* +0x20 the Overbridge id: stock uses 0..387 */
    unsigned int modmask;       /* +0x24 modulation destination flags */
    const char *name;           /* +0x28 "Name=value" in the header */
    const char *group_name;     /* +0x2c */
    const char *short_name;     /* +0x30 the label under the knob */
    const void *format;         /* +0x34 void f(int value << 8, char *out) */
    const char *suffix;         /* +0x38 */
};

struct param_row {
    int id;                     /* +0x00 0x141..0x17f */
    struct param_desc d;        /* +0x04 */
    int look;                   /* +0x40 the stock row whose display entry it borrows, or -1 */
    unsigned int flags_set;     /* +0x44 ORed into the borrowed display flags; bit 0 or 3 set
                                 *       = never p-lockable (8 for every send-FX row) */
    unsigned int opts;          /* +0x48 bit 0: keep the borrowed randomiser */
    int page, slot;             /* +0x4c +0x50 a page record and knob (0..7 = A..H), or -1 */
};

#define V(x) ((x) << 8)

/* Stock 1.12 addresses the rows name (checked by dn2-cfw's scripts/t65_make_mod.py) */
#define STR_EMPTY       ((const char *)0x40218a2a)   /* the empty suffix of every row */
#define STR_MASTER      ((const char *)0x40210b64)   /* "Master": the compressor's group name */
#define STR_CHORUS      ((const char *)0x40210a96)   /* "Chorus" */
#define FMT_NUMBER      ((const void *)0x400e3030)   /* a plain number */

#endif
