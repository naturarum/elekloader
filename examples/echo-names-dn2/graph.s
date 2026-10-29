| SPDX-License-Identifier: GPL-2.0-or-later
| echo-names: the filter picture over knobs E and F of the Echo page (Digitone II OS 1.12).
| ColdFire V4; assemble with -mcpu=54455. EXPERIMENT: emulator-tested only.
|
| FxPageView::vslot04 (0x4006f5e4) draws the SEND FX pages. Its loop over the eight knobs keeps
| two values for the delay page's picture instead of drawing knobs E and F: row 0x75's value in
| its local sp@(60), row 0x76's in sp@(64) (a switch on the row id at 0x4006fb12; both cleared
| at the routine's start). After the loop it asks the page id (0x4006f70a: movea.l (a2),a0;
| moveq #19,d5; move.l a2,-(sp); movea.l (0x88,a0),a0; jsr (a0)) and, unless it is 0x13 (the
| chorus), draws from those two locals a pass band: a high-pass corner at x = 20 + v60 * 52 >>
| 15, a flat top 52 * v64 >> 15 pixels long after it, a low-pass corner, each slope 10 pixels.
| Nothing else reads the two locals.
|
| For the Echo page, CNTR (row 0x75) is the band's centre and BW (row 0x76) its width, so the
| same picture, given the band's lower edge and its width (fxe_band, echo_names.c), draws the
| tape echo's band filter. fxe_graphid replaces the 6-byte `movea.l (0x88,a0),a0; jsr (a0)`
| at 0x4006f710 with a jsr: it asks the page id as the stock code does, and for the delay page
| (0x11) rewrites the two locals before the picture is drawn. Every other page is untouched.
|
| At entry: (sp) the return address, 4(sp) the view (the caller's a2), and the caller's sp
| inside the routine = sp + 8, so its sp@(60) and sp@(64) are 68(sp) and 72(sp) here.
| Changes d0, d1, a0, a1 (a jsr (a0) to the page-id method may change them too).

        .section .run, "ax"
        .globl  fxe_graphid
fxe_graphid:
        movea.l 4(%sp), %a0             | the view
        movea.l (%a0), %a1              | its vtable
        move.l  %a0, -(%sp)
        movea.l 0x88(%a1), %a1
        jsr     (%a1)                   | d0 = the page id
        addq.l  #4, %sp
        moveq   #0x11, %d1
        cmp.l   %d1, %d0
        bne.s   1f
        pea     72(%sp)                 | the caller's sp@(64): BW in, the band's width out
        pea     72(%sp)                 | the caller's sp@(60) (sp moved by 4): CNTR in, lower edge out
        jsr     fxe_band
        addq.l  #8, %sp
        moveq   #0x11, %d0
1:      rts
