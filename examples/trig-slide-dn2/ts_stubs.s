| SPDX-License-Identifier: GPL-2.0-or-later
| trig-slide (Digitone II OS 1.12): the calls ts.c cannot take as plain C functions (dn2-cfw notes/T2.9-trig-slides.md).
| Each stands in for one 6-byte instruction run (a jsr site) and keeps every register the code after it may still use.

        .text

| 0x40138e3c(event, time) writes only d0 and a0, so its callers may keep values in d1 and a1 across it: the two
| entries keep them.

| in place of the sequencer task's jsr 0x40138e3c at 0x400da184: ts_queue(event, time)
        .globl  ts_queue_hook
ts_queue_hook:
        lea     -8(%sp),%sp
        movem.l %d1/%a1,(%sp)
        move.l  16(%sp),-(%sp)          | time
        move.l  16(%sp),-(%sp)          | event
        jsr     ts_queue
        addq.l  #8,%sp
        movem.l (%sp),%d1/%a1
        lea     8(%sp),%sp
        rts

| play start's: 0x400d7e88 and 0x400d806a load 0x40138e3c into d7 (0x400d7f14, 0x400d8380) and call it through it
| for each track's first trig: ts_start(event, time)
        .globl  ts_start_hook
ts_start_hook:
        lea     -8(%sp),%sp
        movem.l %d1/%a1,(%sp)
        move.l  16(%sp),-(%sp)          | time
        move.l  16(%sp),-(%sp)          | event
        jsr     ts_start
        addq.l  #8,%sp
        movem.l (%sp),%d1/%a1
        lea     8(%sp),%sp
        rts

| SLIDE, the sound's word k 100, kept in the stored sound's free id 107. The stock sound serializer 0x400dd80a copies
| k 0..99 through its map into stored ids 0..106 after clearing ids 0..107; the loader 0x400dd34e clears the live
| words and reads ids 0..106 back.

| in the serializer (A3 the live sound, A2 the stored record), in place of 0x400dd8b2 move.b (0xde,%a3),(0xf4,%a2)
        .globl  ts_ser
ts_ser:
        move.w  0xdc(%a3),0xf2(%a2)     | live k 100 -> stored id 107 (+0x1c + 2 * 107)
        move.b  0xde(%a3),0xf4(%a2)     | the stock instruction, last: the flags are its own
        rts

| in the loader (A3 the stored record, A2 the live sound), in place of 0x400dd3e6 mvs.b (0x1c,%a2),%d0;
| moveq #6,%d2, right after the loop that reads ids 0..106
        .globl  ts_load
ts_load:
        move.w  0xf2(%a3),0xdc(%a2)     | stored id 107 -> live k 100
        mvs.b   0x1c(%a2),%d0           | the stock instructions
        moveq   #6,%d2
        rts

| SLIDE's p-locks (0.2): the pattern stores its lock lanes by stored id too, through the same k -> id map, which ends
| at k 99. In the lock serializer 0x400de87c, in place of lea (0x401fd330).l,a3: a3 = ts_map, the stock map with
| k 100 -> 107 (filled from it the first time); every other register as it was.
        .globl  ts_lmap
ts_lmap:
        lea     -16(%sp),%sp
        movem.l %d0-%d1/%a0-%a1,(%sp)
        jsr     ts_map_fill
        movea.l %d0,%a3
        movem.l (%sp),%d0-%d1/%a0-%a1
        lea     16(%sp),%sp
        rts

| In the lock loader 0x400de682, in place of clr.l d2; lea (-0x18cc,pc),a5 (the converter 0x400dce24 it calls through
| a5 for each lane): d2 = 0, a5 = ts_lconv (stored id 107 -> SLIDE, the others through 0x400dce24). Its test
| "stored id < 107" becomes "< 108" (moveq #0x6a -> #0x6b at 0x400de6f8): a stock OS drops a lane with id 107.
        .globl  ts_lpre
ts_lpre:
        clr.l   %d2
        lea     ts_lconv,%a5
        rts
