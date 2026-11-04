| SPDX-License-Identifier: GPL-2.0-or-later
| usb-multi (Digitone II OS 1.12): the code um.c cannot be as plain C functions (dn2-cfw
| notes/USB01-class-compliant-multichannel.md). The four menu callbacks are USB AUDIO/MIDI's own (0x401047d2,
| 0x401052d2, 0x40104d3a, 0x401052ec), instruction for instruction, with our name, choice and test.

        .text

| in place of 0x400040e6's move.l #0x40380000,d0 (FUN_400040a6, mode 5's endpoints): the dQH capabilities of EP 0x83
| IN, one transaction of 56 bytes (stereo) or of 882 (ours); d0 and the flags as that move leaves them
        .globl  um_dqh
um_dqh:
        tst.l   um_on
        bne.s   1f
        move.l  #0x40380000,%d0
        rts
1:      move.l  #0x43720000,%d0
        rts

| in place of 0x40105220's three instructions in 0x40104f42 (the USB CFG menu, after its third item):
| movea.l (a2),a0; clr.l -(sp); move.l d6,-(sp). The fourth item first (a2: the menu); d0, d1, a0 and a1 are not
| read after them before a call.
        .globl  um_menu_hook
um_menu_hook:
        move.l  %a2,-(%sp)
        jsr     um_menu_add
        addq.l  #4,%sp
        movea.l (%sp)+,%a1              | the return address
        clr.l   -(%sp)
        move.l  %d6,-(%sp)
        movea.l (%a2),%a0
        jmp     (%a1)

| the name (0x401047d2): a std::string built in the place a0 gives, from our C string
        .globl  um_inv_name
um_inv_name:
        link.w  %fp,#-4
        move.l  %d2,-(%sp)
        pea     -1(%fp)
        pea     um_name
        move.l  %a0,%d2
        move.l  %a0,-(%sp)
        jsr     0x401ceab8
        lea     12(%sp),%sp
        move.l  %d2,%d0
        move.l  -8(%fp),%d2
        unlk    %fp
        rts

| YES on the item (0x401052d2): the choice, then the menu built again (0x40104f42, its check boxes)
        .globl  um_inv_yes
um_inv_yes:
        move.l  %a2,-(%sp)
        movea.l 8(%sp),%a0
        movea.l (%a0),%a2
        move.l  (%a2),-(%sp)
        jsr     um_choose
        addq.l  #4,%sp
        move.l  (%a2),8(%sp)
        movea.l (%sp)+,%a2
        jmp     0x40104f42

| a turn on the item (0x401052ec): as YES when its third argument is above 0
        .globl  um_inv_turn
um_inv_turn:
        move.l  %a2,-(%sp)
        movea.l 8(%sp),%a0
        movea.l (%a0),%a2
        tst.l   16(%sp)
        ble.s   1f
        move.l  (%a2),-(%sp)
        jsr     um_choose
        addq.l  #4,%sp
        move.l  (%a2),8(%sp)
        movea.l (%sp)+,%a2
        jmp     0x40104f42
1:      movea.l (%sp)+,%a2
        rts

| the check box (0x40104d3a): ticked when USB CONFIG is 3; the stock's drawing call 0x40115adc with its arguments
| rewritten in place, the box's picture 28 bytes on from 0x44645d84's when ticked
        .globl  um_inv_box
um_inv_box:
        lea     -12(%sp),%sp
        movem.l %d2-%d4,(%sp)
        move.l  24(%sp),%d2
        move.l  32(%sp),%d4
        move.l  28(%sp),%d3
        addi.l  #11,%d3
        jsr     um_cfg_get
        move.l  %d4,28(%sp)
        move.l  %d3,24(%sp)
        move.l  %d2,16(%sp)
        moveq   #3,%d1
        cmp.l   %d0,%d1
        seq     %d0
        movem.l (%sp),%d2-%d4
        clr.l   32(%sp)
        mvs.b   %d0,%d0
        neg.l   %d0
        move.l  %d0,%d1
        lsl.l   #2,%d1
        lsl.l   #5,%d0
        sub.l   %d1,%d0
        add.l   0x44645d84,%d0
        move.l  %d0,20(%sp)
        lea     12(%sp),%sp
        jmp     0x40115adc
