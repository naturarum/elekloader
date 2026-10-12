| SPDX-License-Identifier: GPL-2.0-or-later
| param-grow (Digitone II OS 1.12): room for 63 more parameter rows.
| ColdFire V4; assemble with -mcpu=54455. EXPERIMENT: emulator-tested only
| (dn2-cfw (github.com/naturarum/dn2-cfw) notes/T6.5-table-growth.md).
|
| Every knob, MIDI CC, p-lock and value the OS shows comes from one table of
| parameter descriptors in the image, 0x141 rows of 0x3c bytes at
| 0x401f83a4. It cannot grow where it is: other tables follow it. This mod
| makes a copy of it in RAM with room for 0x180 rows, points the OS at the
| copy, and resizes the two structures that are sized by the row count. On
| its own it adds no parameter: the 63 rows after the stock ones are copies of
| row 0 ("Error", in no map and on no page), which is what the stock OS reads
| for any row id it clamps. Other mods add rows through the table param_rows.
|
| What changes (mod.json's sites; dn2-cfw's scripts/t65_make_mod.py finds
| each one in the stock image and checks its bytes):
|
| - 56 instructions that hold the table's address (or row 0's +0x08/+0x20)
|   now hold pg_desc's (keep2: the opcode word stays, the operand changes).
| - 57 compares of a row id with the row count (#0x141, or #0x140 for <=)
|   now compare with 0x180 / 0x17f. Five compares that look the same but are
|   not the row count stay: two row-id tests in the filter page (0x4006ef0a,
|   0x4006ef14) and a 16 x 0x14 loop in three KitActive functions.
| - The ParameterUpdateCache (MIDI CC/NRPN in), 0xfff4 bytes on the heap:
|   values [2][17][rows] and dirty bits [2][17][rows words], so 33
|   constants in its five functions follow the row count (0x282 -> 0x300,
|   0x2aa2 -> 0x3300, 0x504 -> 0x600, 0x5544 -> 0x6600, 0x555c -> 0x6618,
|   0xffe4.. -> 0x13218.., size 0xfff4 -> 0x13228). Same formulas, N = 0x180.
| - The display table (flags, glyph, randomiser, value text; 0x44 bytes per
|   row) is built in RAM by a static initializer with 1,633 absolute writes,
|   and read only through 0x400c257c. That reader now jumps to pg_disp_get:
|   stock rows read the stock entries where they are, a new row reads its
|   own entry if a mod gave it one, anything else reads row 0's (as stock).
|
| When (the boot order; nothing reads the table before it is in place):
| - pg_boot runs from the OS entry's call into the C start (0x40000568,
|   jsr 0x40186fa2), after core's boot copy, the SRAM copies and the DDR
|   clear, before any of the OS's own code. The stock table is copied from
|   the image as loaded, with every mod's data sites in it (fx-rename's
|   names, for one), then the new rows go in.
| - pg_sinit runs where an init_array function calls the static initializer
|   that builds the display table and the page records (0x401ae0cc, jsr
|   0x400c2c4e): after it, each new row's display entry is copied from the
|   stock row it borrows, and a row that asks is put on a page's empty slot.
|
| A row a mod adds (table param_rows: one pointer per mod to an array of
| these, ended by an id of 0):
|   +0x00 id        0x141..0x17f; claim it as the resource "param-row:<id>"
|   +0x04 desc      the 0x3c-byte descriptor, in the stock layout: group, k,
|                   min, max, default (<< 8), bipolar, CC, CC2 (s16, -1 none),
|                   NRPN (-1 none), Overbridge id, modulation mask, long name,
|                   group name, short name, formatter, suffix
|   +0x40 look      the stock row whose display entry it borrows (glyph,
|                   value text, flags), or -1 for none (row 0's)
|   +0x44 flags_set bits ORed into the borrowed flags. A row is p-lockable
|                   when bits 0 and 3 are both clear (0x40064ccc); a send-FX
|                   row must never be (its lock would be saved as a lock on
|                   the track's sound slot with the same k): give it 8
|   +0x48 opts      bit 0: keep the borrowed randomiser (default: none; the
|                   sound randomiser 0x4004b0b4 writes sound slot k for every
|                   row that has one)
|   +0x4c page      a page record 0..36 to put the row on at boot, or -1
|   +0x50 slot      its knob there, 0..7 = A..H; only an empty slot is used
| See dn2-cfw's notes/T6.1-table-growth-study.md section 3.4 for the rules a
| new row must follow (a free k in its set, no CC the set's map already has,
| an Overbridge id outside 0..387, 0 as the classic value).

        .equ    DESC,     0x401f83a4    | the stock descriptor table (image)
        .equ    ROW,      0x3c
        .equ    ROWS0,    0x141         | stock rows
        .equ    ROWS,     0x180         | rows with this mod
        .equ    NEW,      ROWS - ROWS0  | 63
        .equ    DISP,     0x4243425c    | the stock display table (RAM)
        .equ    XROW,     0x44
        .equ    PAGES,    0x42433c00    | page records: +8 the eight row ids
        .equ    PAGE,     0x2c
        .equ    NPAGES,   37
        .equ    CSTART,   0x40186fa2    | what the OS entry calls at 0x40000568
        .equ    SINIT,    0x400c2c4e    | the display table's static initializer
        | struct param_row
        .equ    R_ID,     0x00
        .equ    R_DESC,   0x04
        .equ    R_LOOK,   0x40
        .equ    R_FSET,   0x44
        .equ    R_OPTS,   0x48
        .equ    R_PAGE,   0x4c
        .equ    R_SLOT,   0x50
        .equ    R_SIZE,   0x54

| Its RAM: a region of the free DDR the device profile gives mods (mod.json claims it), at
| a fixed address so that adding row mods moves nothing. Stock never touches it (dn2-cfw
| notes/T1.3 and T1.9); DDR holds noise at power-on, so pg_boot writes all of it it reads.
        .equ    PG_BASE,  0x47D00000    | ..0x47D10000
        .globl  pg_desc, pg_xdisp, pg_xdisp_on
        .set    pg_desc,     PG_BASE                    | the table the OS reads (0x5a00)
        .set    pg_xdisp,    PG_BASE + ROWS * ROW       | display entries of new rows (0x10bc)
        .set    pg_xdisp_on, PG_BASE + 0x6b00 - 0x40    | 1: the new row has its own entry

        .section .run, "ax"

| ---- pg_boot: at 0x40000568, in place of jsr 0x40186fa2 --------------------
| (sp) = 0x4000056e. Interrupts are masked, the caches are on. Ends with a
| jmp to the C start, so its rts returns where the stock call would.
        .globl  pg_boot
pg_boot:
        lea     -16(%sp), %sp
        movem.l %d2/%a2-%a4, (%sp)
        | 1. the stock rows, as the image holds them
        lea     DESC, %a0
        lea     pg_desc, %a1
        move.l  #ROWS0 * ROW / 4, %d0
1:      move.l  (%a0)+, (%a1)+
        subq.l  #1, %d0
        bne.s   1b
        | 2. every row after them: a copy of row 0
        moveq   #NEW, %d1
2:      lea     DESC, %a0
        moveq   #ROW / 4, %d0
3:      move.l  (%a0)+, (%a1)+
        subq.l  #1, %d0
        bne.s   3b
        subq.l  #1, %d1
        bne.s   2b
        | 3. no new row has a display entry of its own until pg_sinit gives it one
        lea     pg_xdisp_on, %a1
        moveq   #0x40 / 4, %d0
21:     clr.l   (%a1)+
        subq.l  #1, %d0
        bne.s   21b
        | 4. the rows mods add
        lea     param_rows, %a2
4:      move.l  (%a2)+, %d0             | a mod's array, or the table's end
        beq.s   8f
        movea.l %d0, %a3
5:      move.l  R_ID(%a3), %d0
        beq.s   4b                      | the array's end
        subi.l  #ROWS0, %d0
        cmpi.l  #NEW, %d0
        bcc.s   7f                      | not a new row's id: left out
        moveq   #ROW, %d1
        muls.l  %d1, %d0
        lea     pg_desc + ROWS0 * ROW, %a1
        adda.l  %d0, %a1
        lea     R_DESC(%a3), %a0
        moveq   #ROW / 4, %d0
6:      move.l  (%a0)+, (%a1)+
        subq.l  #1, %d0
        bne.s   6b
7:      lea     R_SIZE(%a3), %a3
        bra.s   5b
8:      movem.l (%sp), %d2/%a2-%a4
        lea     16(%sp), %sp
        jmp     CSTART

| ---- pg_disp_get: the display table's accessor 0x400c257c (by jmp) ---------
| 4(sp) = a row id -> d0 = its display entry. Changes d0, d1 and a0 (the
| stock accessor changes d0, d1, a0 and a1).
        .globl  pg_disp_get
pg_disp_get:
        move.l  4(%sp), %d0
        cmpi.l  #ROWS0, %d0
        bcc.s   1f
        moveq   #XROW, %d1              | a stock row: its stock entry
        muls.l  %d1, %d0
        addi.l  #DISP, %d0
        rts
1:      subi.l  #ROWS0, %d0
        cmpi.l  #NEW, %d0
        bcc.s   2f
        lea     pg_xdisp_on, %a0
        tst.b   (%a0,%d0.l)
        beq.s   2f
        moveq   #XROW, %d1              | a new row with its own entry
        muls.l  %d1, %d0
        addi.l  #pg_xdisp, %d0
        rts
2:      move.l  #DISP, %d0              | row 0's, as the stock clamp
        rts

| ---- pg_sinit: at 0x401ae0cc, in place of jsr 0x400c2c4e -------------------
| (sp) = 0x401ae0d2, 4(sp) = the argument FUN_401ae0c8 pushed (1). The
| static initializer builds the display table and the page records; then
| each new row gets its display entry and, if it asks, its place on a page.
        .globl  pg_sinit
pg_sinit:
        move.l  4(%sp), -(%sp)
        jsr     SINIT
        addq.l  #4, %sp
        lea     -16(%sp), %sp
        movem.l %d2-%d3/%a2-%a3, (%sp)
        lea     param_rows, %a2
1:      move.l  (%a2)+, %d0
        beq.w   9f
        movea.l %d0, %a3
2:      move.l  R_ID(%a3), %d2
        beq.w   1b
        subi.l  #ROWS0, %d2             | d2 = index of the new row
        cmpi.l  #NEW, %d2
        bcc.w   8f
        move.l  R_LOOK(%a3), %d0
        cmpi.l  #ROWS0, %d0
        bcc.w   5f                      | borrows none: reads row 0's
        moveq   #XROW, %d1
        muls.l  %d1, %d0
        addi.l  #DISP, %d0
        movea.l %d0, %a0                | the stock entry it borrows
        move.l  %d2, %d0
        muls.l  %d1, %d0
        addi.l  #pg_xdisp, %d0
        movea.l %d0, %a1                | its own
        moveq   #XROW / 4, %d0
3:      move.l  (%a0)+, (%a1)+
        subq.l  #1, %d0
        bne.s   3b
        lea     -XROW(%a1), %a1
        move.l  R_FSET(%a3), %d0
        or.l    %d0, (%a1)              | +0x00 flags
        move.l  R_OPTS(%a3), %d0
        btst    #0, %d0
        bne.s   4f
        clr.l   0x24(%a1)               | +0x24 the randomiser: an empty
        clr.l   0x28(%a1)               | std::function (functor, manager,
        clr.l   0x2c(%a1)               | invoker all 0)
        clr.l   0x30(%a1)
4:      lea     pg_xdisp_on, %a0
        moveq   #1, %d0
        move.b  %d0, (%a0,%d2.l)
5:      move.l  R_PAGE(%a3), %d0
        cmpi.l  #NPAGES, %d0
        bcc.w   8f                      | -1: on no page
        move.l  R_SLOT(%a3), %d1
        cmpi.l  #8, %d1
        bcc.w   8f
        moveq   #PAGE, %d3
        muls.l  %d3, %d0
        lsl.l   #2, %d1
        add.l   %d1, %d0
        addi.l  #PAGES + 8, %d0
        movea.l %d0, %a0
        tst.l   (%a0)
        bne.w   8f                      | a stock knob is there: left as it is
        move.l  R_ID(%a3), (%a0)
8:      lea     R_SIZE(%a3), %a3
        bra.w   2b
9:      movem.l (%sp), %d2-%d3/%a2-%a3
        lea     16(%sp), %sp
        rts
