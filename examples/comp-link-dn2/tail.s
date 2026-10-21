| SPDX-License-Identifier: GPL-2.0-or-later
| comp-link-dn2 (Digitone II OS 1.12; 0.1, unchanged in 0.2): the 3-band
| compressor's band knobs to the DSP, in the tail of the SPI frame. ColdFire V4;
| assemble with -mcpu=54455. 0.2 adds the kit storage (store.s, store.c).
|
| The vector-191 frame handler FUN_40025e36 builds the frame to the DSP and, at
| 0x40025e9e, calls the DSPI2 driver:
|     jsr 0x400cf922       FUN_400cf922(0xa80, 0x80005e60, 0xabc, 0x800053a4)
| The driver sends 0xabc bytes from the PUSHR staging entries at 0x80002510
| (4 bytes each: a command half, then a data half), but copies only the 0xa80
| payload bytes into their data halves. The data halves of the last 30 entries
| (0x540..0x55d, frame bytes 0xa80..0xabb) go out as whatever they hold: zeros
| on a stock unit, and no stock DSP code reads them.
|
| compl_tail takes the driver's place at that call (a jsr to it). It is entered
| with the driver's four arguments on the stack, as the driver would be, writes
| the 30 tail words into those data halves, then jumps to the driver with the
| stack untouched, so the driver's rts returns into the handler. It writes only
| when DSPI2 SR bit 28 (EOQF) says the previous transfer has finished, the
| condition the driver itself starts a new transfer on, so it never changes a
| frame under the DMA. d0, d1, a0 and a1 are free (the driver is a C call); d2
| is saved. In .run (DDR): the ISR can run before core's first ev_tick copies
| .fast.
|
| Tail layout (word i = frame byte 0xa80 + 2i):
|   0      0xC3B1: magic, the low byte is the layout (1)
|   1      sequence, +1 per tail written
|   2..28  the smoothed FX row-16 words of k 70..96 (0x800068e4 + 0xcc2 + 2k;
|          k 87..89 the band mixes of param-rows 0.3, k 90..96 spare),
|          which the stock smoother FUN_400db28e glides as it glides the
|          framed k 25..69 (value << 8)
|   29     ~(sum of words 0..28), low 16 bits: an all-zero tail never passes
| The DSP side is dn2-cfw's CMP2 (tools/dspsplice/src/cmp_tail.c).
        .equ    DSPI2_SR, 0xEC03802C
        .equ    DRIVER,   0x400cf922
        .equ    SMOOTH16, 0x800075a6        | 0x800068e4 + 0xcc2: smoothed row 16, word k at +2k
        .equ    TAIL0,    0x80003a12        | data half of PUSHR entry 0x540
        .equ    K0,       70
        .equ    NV,       27
        .equ    MAGIC,    0xC3B1

        .section .run, "ax"
        .globl  compl_tail
compl_tail:
        move.l  DSPI2_SR, %d0
        btst    #28, %d0
        beq.s   9f                          | a transfer is running: write nothing
        move.l  %d2, -(%sp)
        lea     TAIL0, %a1
        move.l  #MAGIC, %d1                 | the running sum
        move.w  %d1, (%a1)
        move.l  compl_seq, %d0
        addq.l  #1, %d0
        move.l  %d0, compl_seq
        move.w  %d0, 4(%a1)
        andi.l  #0xffff, %d0
        add.l   %d0, %d1
        lea     SMOOTH16+2*K0, %a0
        addq.l  #8, %a1
        moveq   #NV-1, %d2
1:      moveq   #0, %d0
        move.w  (%a0)+, %d0
        move.w  %d0, (%a1)
        addq.l  #4, %a1
        add.l   %d0, %d1
        subq.l  #1, %d2
        bpl.s   1b
        not.l   %d1
        move.w  %d1, (%a1)                  | word 29, at entry 0x55d
        move.l  (%sp)+, %d2
9:      jmp     DRIVER

        .section .bss
        .balign 4
        .globl  compl_seq
compl_seq:
        .space  4
