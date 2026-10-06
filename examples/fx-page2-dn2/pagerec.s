| SPDX-License-Identifier: GPL-2.0-or-later
| fx-page2: the page-record accessor, replacing 0x400c25d8 (Digitone II OS 1.12) by a jmp.
|
| Stock: rec(id) = 0x42433c00 + 0x2c * id for id 0..36, else the record before the table
| (id -1). It is a direct call from 20 sites, and it only changes d0 and d1, so callers may
| keep values in a0/a1 across it: this one changes d0 and d1 only, too. FX2_ID (37), the
| new page, returns this mod's record; every other id behaves as stock.
        .section .run, "ax"
        .globl  fx2_pagerec
fx2_pagerec:
        move.l  4(%sp),%d0
        moveq   #37,%d1
        cmp.l   %d1,%d0
        bne.s   1f
        move.l  #fx2_rec,%d0
        rts
1:      moveq   #36,%d1
        cmp.l   %d0,%d1
        bcc.s   2f
        moveq   #-1,%d0
2:      moveq   #44,%d1
        muls.l  %d1,%d0
        addi.l  #0x42433c00,%d0
        rts
