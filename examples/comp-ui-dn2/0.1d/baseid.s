| SPDX-License-Identifier: GPL-2.0-or-later
| comp-ui (Digitone II OS 1.12): the band pages count as the Compressor page in
| MasterPageView::vslot47. ColdFire V4; assemble with -mcpu=54455.
|
| vslot47 (0x4006ffb0) gives the row behind a knob: it takes the page's row
| (ParameterPageView::vslot47), then asks the view for its page id and, on
| page 0x14 (the stock Compressor page) with DUAL mode on (row 0x94), shows
| SCS's DUAL row 0x9c (.. In L, In R) in place of 0x9b (.. In LR). At
| 0x4006ffc8 it does, with a0 = the view's vtable and the view pushed:
|     movea.l 0x88(a0),a0 ; move.l d0,d2 ; jsr (a0)
| A jsr to compui_baseid (and a nop) replaces those three instructions. It
| does their work and answers 0x14 for the band pages 40..42, so their G knob
| shows row 0x9c in DUAL mode as on the stock page. Nothing else in vslot47
| depends on page 0x14. As stock: d2 = the row, d0 = the page id, and d1, a0
| and a1 are free (the stock call clobbers them too).
        .section .run, "ax"
        .globl  compui_baseid
compui_baseid:
        move.l  %d0, %d2                | the row
        movea.l 0x88(%a0), %a0          | getPageId
        move.l  4(%sp), -(%sp)          | the view, as the stock call had it
        jsr     (%a0)
        addq.l  #4, %sp
        moveq   #40, %d1
        cmp.l   %d1, %d0
        blt.s   1f
        moveq   #42, %d1
        cmp.l   %d1, %d0
        bgt.s   1f
        moveq   #0x14, %d0              | a band page: the Compressor page's id
1:      rts
