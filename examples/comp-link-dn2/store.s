| SPDX-License-Identifier: GPL-2.0-or-later
| comp-link-dn2 0.2 (Digitone II OS 1.12): the entry hooks of the FX record
| serializer and loader, for the band values saved per kit (store.c).
| ColdFire V4; assemble with -mcpu=54455.
|
| site 0x400dcf34, FUN_400dcf34(stored, live) -> d0.b 1 ok / 0: the only live ->
| stored path of an FX setup (callers: the kit serializer FUN_400ddfa8 at
| 0x400de034, jsr pc-relative, and the mirror job FUN_4002fc34 at 0x4002fc50):
|   stock 2f0b 2f0a 246f 000c   move.l a3,-(sp) ; move.l a2,-(sp) ; movea.l 12(sp),a2
|   new   4ef9 <compl_ser> 4e71
| site 0x400dcef0, FUN_400dcef0(live, stored) -> d0.b: the only stored -> live
| path (callers: the kit loader FUN_400dddb6 at 0x400dde90, which calls the row
| defaults FUN_400e7140 when it returns 0, as it does for a record whose version
| word is not 0; and FUN_400dfb34's bra.w at 0x400dfb66, which nothing references):
|   stock 2f0b 2f0a 226f 000c   move.l a3,-(sp) ; move.l a2,-(sp) ; movea.l 12(sp),a1
|   new   4ef9 <compl_load> 4e71
| No branch in either function targets its first 8 bytes. Each hook calls the
| stock body (the displaced instructions, then a jmp to its 9th byte) with the
| caller's two arguments, keeps its d0, and adds the extra words on success only.
| d0, d1, a0, a1 are free (C calls); the stock body saves a2/a3 itself.
| Prototype: dn2-cfw out/T6.4/store-frame/store/fxs_s.s (T6.4 3.2).
        .section .run, "ax"
        .globl  compl_ser, compl_load
compl_ser:
        move.l  8(%sp), -(%sp)
        move.l  8(%sp), -(%sp)
        bsr.s   1f
        addq.l  #8, %sp
        tst.b   %d0
        beq.s   2f
        move.l  %d0, -(%sp)
        move.l  12(%sp), -(%sp)         | live
        move.l  12(%sp), -(%sp)         | stored
        jsr     compl_encode
        addq.l  #8, %sp
        move.l  (%sp)+, %d0
2:      rts
1:      move.l  %a3, -(%sp)
        move.l  %a2, -(%sp)
        movea.l 12(%sp), %a2
        jmp     0x400dcf3c              | movea.l 16(sp),a3: the 9th byte

compl_load:
        move.l  8(%sp), -(%sp)
        move.l  8(%sp), -(%sp)
        bsr.s   1f
        addq.l  #8, %sp
        tst.b   %d0
        beq.s   2f
        move.l  %d0, -(%sp)
        move.l  12(%sp), -(%sp)         | stored
        move.l  12(%sp), -(%sp)         | live
        jsr     compl_decode
        addq.l  #8, %sp
        move.l  (%sp)+, %d0
2:      rts
1:      move.l  %a3, -(%sp)
        move.l  %a2, -(%sp)
        movea.l 12(%sp), %a1
        jmp     0x400dcef8              | movea.l 16(sp),a0: the 9th byte
