| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2 0.2: page records for mods (Digitone II OS 1.12). ColdFire V4;
| assemble with -mcpu=54455.
|
| A parameter page (a SYN, FLTR, FX or MIXER page: the eight knobs and
| LEVEL) is a 44-byte record: +0 the short title and +4 the long title (COW
| std::string data pointers), +8 the row ids of knobs A..H (0 = an empty
| box), +0x28 LEVEL's row. The OS finds a record by its page id through one
| routine, PAGE_REC (0x400c25d8, 20 direct callers): ids 0..36 are the stock
| records, PAGE_RECS + 44 x id, which the static initializer 0x400c2c4e builds
| at boot; any other id gets the record before the table (id -1). Two mods
| cannot both patch that routine, so core owns it from 0.2 (a jmp at its
| entry) and serves every id a mod registers:
|
| - ids 0..36 go the stock way, instruction for instruction (the same d0,
|   and d1 = 44, as stock leaves it);
| - an id in the table core_pagerecs returns the record its mod gave;
| - any other id gets the record before the table, as stock.
|
| A mod registers a page with an 8-byte entry {id, record} (mod.json):
|   "contribute": [{"to": "core_pagerecs", "order": 50, "data": "0000002800000000",
|                   "relocs": [[4, "abs32", "sym:my_rec", 0]]}],
|   "resources": {"names": ["page:40"]}
| Use an id from 37 up and claim it as the resource "page:<id>" (the record
| itself in the mod's .run, in the stock layout). The table ends with id 0.
| Ids already in use: 37 (examples/fx-page2-dn2, which patches PAGE_REC
| itself and so cannot be combined with core 0.2), 40-42 (comp-ui-dn2).
|
| The stock routine changes only d0 and d1, and its callers keep values in
| a0 and a1 across it; this one keeps a0 on the stack while it walks the
| table, so it too changes only d0 and d1. In .run (DDR): the OS builds its
| views, which ask for records, before the first ev_tick copies .fast.
        .section .run, "ax"
        .globl  core_pagerec
core_pagerec:
        move.l  4(%sp), %d0             | the page id
        moveq   #36, %d1
        cmp.l   %d0, %d1
        bcc.s   3f                      | 0..36: a stock page, as stock
        move.l  %a0, -(%sp)
        lea     core_pagerecs, %a0
1:      move.l  (%a0)+, %d1             | an entry's id; 0 ends the table
        beq.s   2f
        cmp.l   %d1, %d0
        beq.s   4f
        addq.l  #4, %a0                 | past its record
        bra.s   1b
4:      move.l  (%a0), %d0              | a mod's page: its record
        movea.l (%sp)+, %a0
        rts
2:      movea.l (%sp)+, %a0
        moveq   #-1, %d0                | no one's: the record before the table
3:      moveq   #44, %d1
        muls.l  %d1, %d0
        addi.l  #PAGE_RECS, %d0
        rts
