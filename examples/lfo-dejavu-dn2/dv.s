| SPDX-License-Identifier: GPL-2.0-or-later
| lfo-dejavu-dn2 0.2 (Digitone II OS 1.12): the entries from the two LFO evaluators into dejavu.c, and (0.2) the
| trig modes ONE and HALF run one cycle, or half a cycle, from the start phase, then hold (lfx_*, at the end).
| ColdFire V4; assemble with -mcpu=54455. Every site was read instruction by instruction and is checked against
| the stock bytes by dn2-cfw's scripts/dj01_make_mod.py. The LFO code itself stays stock: these stubs only pick
| what it reads for MODE on the added waves, give it our waveform for WAVE 7..15, and round the result on the
| scale waves (9..16; 16, KEY, in the track's own key).
|
| Evaluator A, FUN_40137a06 (every voice row's three LFOs, once per frame, called only from the frame handler):
|   0x400272d4  jsr 0x40137a06                       -> jsr dv_frame_hook (counts frames, then the stock call)
|   0x40137b60  moveq #6,d7 ; 0x40137b66 moveq #6,d3  the WAVE clamp 0..6 -> 0..16 (data: 7e06 -> 7e10, 7606 -> 7610)
|   0x40137b88  mvs.b 0x50(a4),d1 ; move.l d6,d2      -> jsr dv_mode_a: MODE, or 0 (FREE) when WAVE >= 7
|   0x40137cda  lea 0x4020b7f8,a0 (the waveform table) -> lea dv_wtab_a (keep2)
|   0x40137d98  mvs.w (a6),d1 ; add.l d1,d0 ; bpl     -> jmp dv_add_a: the stock add and clamp, the scale's
|               rounding on WAVE 9..15, the stock store, back at 0x40137db2
| Evaluator B, FUN_401376bc (a MIDI track's three LFOs, called by the MIDI task):
|   0x40137770  mvs.b 0x2e(a4),d7 ; mvs.b 0x2a(a4),d3 -> jsr dv_mode_b ; nop
|   0x40137788  moveq #6,d6 ; 0x4013778e moveq #6,d3  the WAVE clamp 0..6 -> 0..16 (7c06 -> 7c10, 7606 -> 7610)
|   0x401378ce  lea 0x4020b7f8,a1                      -> lea dv_wtab_b (keep2)
| Registers at A's waveform call (0x40137ce6, jsr (a0) with the scaled phase on the stack): a2 + 0x50 = the LFO's
| state (its phase already stored), a4 = the LFO's parameters in the smoothed row (SPH 0x4e, MODE 0x50, the
| stock A4 - 0x10 per LFO), d3 = WAVE, a5 = the voice row; in the caller's frame (0x4c bytes of locals, +8 here
| for its push and the return address) the trig flag local_20 at 0x2c and the LFO local_1c (2, 1, 0) at 0x30.
| At B's (0x401378f0, jsr (a1)): a2 + 0x50 = the state, a4 = the parameters (SPH 0x2c, MODE 0x2e), d3 = WAVE,
| d5 = the trig flag; in its frame (0x3c bytes) the LFO local_c at 0x30 and the track, its second argument, at
| 0x44. The callers treat the waveforms as C functions (d0 the result; d1, a0, a1 free; B saves its d1
| itself); so do these stubs.
        .section .run, "ax"
        .globl  dv_frame_hook, dv_mode_a, dv_add_a, dv_mode_b, dv_wtab_a, dv_wtab_b

dv_frame_hook:
        addq.l  #1, dv_frame
        jmp     0x40137a06

| d3 = WAVE (0..15 after the clamp); d1 = MODE as stock (mvs.b), or 0 on the added waves; d2 = d6, as stock
dv_mode_a:
        move.l  %d6, %d2
        moveq   #7, %d1
        cmp.l   %d1, %d3
        bge.s   1f
        mvs.b   0x50(%a4), %d1
        rts
1:      moveq   #0, %d1
        rts

dv_wave_a_s:
        move.l  0x38(%sp), -(%sp)       | the LFO, 2..0 (the caller's local_1c)
        move.l  0x38(%sp), -(%sp)       | trig (the caller's local_20, 0x34(sp) before the push)
        mvs.b   0x50(%a4), %d0
        move.l  %d0, -(%sp)             | MODE: the length
        mvs.w   0x4e(%a4), %d0
        asr.l   #8, %d0
        move.l  %d0, -(%sp)             | SPH: the mutation
        move.l  %d3, -(%sp)             | WAVE
        move.l  %a5, -(%sp)             | the voice row
        pea     0x50(%a2)               | the LFO's state
        jsr     dv_wave_a
        lea     28(%sp), %sp
        rts

| d0 = the LFO's amount, a6 = &row[DEST], d7 = DEST, d3 = WAVE, a5 = the voice row
dv_add_a:
        mvs.w   (%a6), %d1
        add.l   %d1, %d0
        bpl.s   1f
        moveq   #0, %d0
        bra.s   2f
1:      cmpi.l  #0x7f00, %d0
        ble.s   2f
        move.l  #0x7f00, %d0
2:      moveq   #9, %d1
        cmp.l   %d1, %d3
        blt.s   3f
        move.l  %a5, -(%sp)             | the voice row
        move.l  %d7, -(%sp)             | DEST
        move.l  %d3, -(%sp)             | WAVE
        move.l  %d0, -(%sp)             | the value
        jsr     dv_quant_a
        lea     16(%sp), %sp
3:      move.w  %d0, (%a6)
        jmp     0x40137db2

| d3 = WAVE (as stock, mvs.b); d7 = MODE as stock, or 0 on the added waves
dv_mode_b:
        mvs.b   0x2a(%a4), %d3
        moveq   #7, %d7
        cmp.l   %d7, %d3
        bge.s   1f
        mvs.b   0x2e(%a4), %d7
        rts
1:      moveq   #0, %d7
        rts

dv_wave_b_s:
        move.l  0x38(%sp), -(%sp)       | the LFO, 2..0 (the caller's local_c)
        move.l  %d5, -(%sp)             | trig
        mvs.b   0x2e(%a4), %d0
        move.l  %d0, -(%sp)             | MODE: the length
        mvs.w   0x2c(%a4), %d0
        asr.l   #8, %d0
        move.l  %d0, -(%sp)             | SPH: the mutation
        move.l  %d3, -(%sp)             | WAVE
        move.l  0x60(%sp), -(%sp)       | the track (the caller's second argument: 0x4c(sp) + the five pushes)
        pea     0x50(%a2)               | the LFO's state
        jsr     dv_wave_b
        lea     28(%sp), %sp
        rts

| the waveform tables: the stock six (TRI SINE SQR SAW EXPO RAMP, 0x4020b7f8's entries), RAND's place (never
| called: RAND has its own code in both evaluators), then the added waves
        .balign 4
dv_wtab_a:
        .long   0x4013753e, 0x40137554, 0x40137520, 0x40137532, 0x401375ae, 0x4013759e, 0
        .long   dv_wave_a_s, dv_wave_a_s, dv_wave_a_s, dv_wave_a_s, dv_wave_a_s, dv_wave_a_s, dv_wave_a_s
        .long   dv_wave_a_s, dv_wave_a_s, dv_wave_a_s
dv_wtab_b:
        .long   0x4013753e, 0x40137554, 0x40137520, 0x40137532, 0x401375ae, 0x4013759e, 0
        .long   dv_wave_b_s, dv_wave_b_s, dv_wave_b_s, dv_wave_b_s, dv_wave_b_s, dv_wave_b_s, dv_wave_b_s
        .long   dv_wave_b_s, dv_wave_b_s, dv_wave_b_s

| ---- 0.2: ONE and HALF from the start phase, then hold
|
| Stock stops ONE at the end of the cycle and HALF at its middle, fixed points: with SPH past 0 an LFO started at SPH
| runs short (or, for HALF, a whole cycle), and at the stop it jumps to a fixed value per waveform (the tables
| 0x4020b7a4, 0x4020b7c0, 0x4020b7dc) instead of keeping where it got to. (Research findings of dn2_firmware_explore,
| angellinares, on OS 1.11: the same code on 1.12, 0x2e0 later; this is our own code.)
|
| The fix moves the phase's origin: at a trig in ONE or HALF on a stock wave (0..5) the phase starts at 0 and SPH's
| cell is latched; the waveform (and the page's phase copy) are read at the phase + the latched start point, the cycle
| wrapped. The stock stop tests then fall one cycle or half a cycle after the trig, in both directions, and the start
| point is the stock one (cell x cycle / 2^15, as the trig's fractional macl, rounded down). In any other mode, or on
| another wave, a latch left over is folded into the phase once (so the LFO goes on where it was) and cleared. At the
| stop the four stores of the fixed values become nops, so the LFO holds its last value.
|
| The latch: A, the record's +0x26 word (never written by stock code; Deja vu keeps its read position there, with
| bit 15 always clear), 0x8000 | the cell. B (MIDI tracks, one record per track; B's +0x26 is its hold flag):
| lfx_lat_b (dejavu.c), a word per 8 bytes of B's block. Both are zero after power-on (the DDR clear, the .bss).
|
| A: 0x40137c40 (moveq #6,d0; cmp.l d3,d0; beq.s: RAND?)      -> jmp lfx_trig_a
|    0x40137ccc (move.l #1667999861,d0: the phase's scale)     -> jsr lfx_eval_a
|    0x40137dbc (move.l 0x50(a2),0x70(a2): the page's copy)   -> jsr lfx_page_a
|    0x40137bcc, 0x40137c00 (the stop values into the output)  -> nops
| B: 0x40137836 (moveq #6,d2; cmp.l d3,d2; beq.s)             -> jmp lfx_trig_b
|    0x401378d8 (muls.l d0,d2; move.l #2135039823,d0)          -> jsr lfx_eval_b + nop nop
|    0x401379d8 (move.l 0x50(a2),0x70(a2))                     -> jsr lfx_page_b
|    0x401377c4, 0x401377fe (the stop values)                  -> nops
| Registers there: d3 = WAVE (0..15, A; B's page site has DEST in d3, so it reads WAVE again), d4 = MODE, a2 + 0x50 =
| the LFO's record (+0 the phase, +0x20 the page's copy), A's SPH long at 0x44(sp) in its frame, B's SPH word at
| 0x2c(a4). Free at every site: d0, d1, a0 (B's eval site: d1 and a1 are live, kept).
        .globl  lfx_trig_a, lfx_eval_a, lfx_page_a, lfx_trig_b, lfx_eval_b, lfx_page_b

lfx_trig_a:
        move.w  0x76(%a2), %d0          | a trig in TRIG mode, or on RAND, starts from SPH as stock: a latch (bit 15)
        bpl.s   0f                      | is dropped; Deja vu's read position (bit 15 clear) is left alone
        clr.w   0x76(%a2)
0:      moveq   #6, %d0
        cmp.l   %d3, %d0
        beq.s   1f
        moveq   #3, %d0
        cmp.l   %d4, %d0
        beq.s   2f
        moveq   #4, %d0
        cmp.l   %d4, %d0
        beq.s   2f
        jmp     0x40137c46              | TRIG: the stock start point into d2
2:      move.l  0x44(%sp), %d0          | ONE, HALF: latch SPH's cell, the phase from 0
        and.l   #0x7fff, %d0
        ori.l   #0x8000, %d0
        move.w  %d0, 0x76(%a2)
1:      moveq   #0, %d2
        jmp     0x40137c5e

| d1 = a latch word (bit 15 set), d2 = a phase -> d2 = the phase + the start point, below the cycle; d1 changed
lfx_shift_a:
        move.l  %d0, -(%sp)
        and.l   #0x7fff, %d1
        move.l  #84375, %d0             | 1,382,400,000 / 2^15 = 84,375 / 2
        mulu.l  %d0, %d1
        lsr.l   #1, %d1
        add.l   %d1, %d2                | both below the cycle: the sum fits 32 bits, unsigned
        cmpi.l  #1382400000, %d2
        bcs.s   1f
        subi.l  #1382400000, %d2
1:      move.l  (%sp)+, %d0
        rts

lfx_eval_a:
        move.w  0x76(%a2), %d1
        bpl.s   9f
        bsr.w   lfx_shift_a
        moveq   #5, %d0
        cmp.l   %d0, %d3
        bhi.s   2f                      | WAVE 7..15: fold
        moveq   #3, %d0
        cmp.l   %d4, %d0
        beq.s   9f                      | ONE: read the waveform at the shifted phase
        moveq   #4, %d0
        cmp.l   %d4, %d0
        beq.s   9f                      | HALF: the same
2:      move.l  %d2, 0x50(%a2)          | another mode: the shifted phase from now on
        clr.w   0x76(%a2)
9:      move.l  #1667999861, %d0
        rts

lfx_page_a:
        move.l  %d2, -(%sp)
        move.l  0x50(%a2), %d2
        move.w  0x76(%a2), %d1
        bpl.s   1f
        bsr.w   lfx_shift_a
        moveq   #5, %d0
        cmp.l   %d0, %d3
        bhi.s   2f                      | RAND (its own code reads no latch) or a Deja vu wave: fold
        moveq   #3, %d0
        cmp.l   %d4, %d0
        beq.s   1f
        moveq   #4, %d0
        cmp.l   %d4, %d0
        beq.s   1f
2:      move.l  %d2, 0x50(%a2)
        clr.w   0x76(%a2)
1:      move.l  %d2, 0x70(%a2)
        move.l  (%sp)+, %d2
        rts

| a2 + 0x50 = a record of B's block (0x44640498 + 0x78 t + 0x28 l) -> a0 = its latch word in lfx_lat_b
lfx_idx_b:
        move.l  %d0, -(%sp)
        move.l  %a2, %d0
        subi.l  #0x44640448, %d0        | the record's place in the block, a multiple of 8
        lsr.l   #2, %d0
        lea     lfx_lat_b, %a0
        adda.l  %d0, %a0
        move.l  (%sp)+, %d0
        rts

| as lfx_shift_a, B's cycle 21,600,000 (/ 2^15 = 84,375 / 2^7)
lfx_shift_b:
        move.l  %d0, -(%sp)
        and.l   #0x7fff, %d1
        move.l  #84375, %d0
        mulu.l  %d0, %d1
        lsr.l   #7, %d1
        add.l   %d1, %d2
        cmpi.l  #21600000, %d2
        bcs.s   1f
        subi.l  #21600000, %d2
1:      move.l  (%sp)+, %d0
        rts

lfx_trig_b:
        bsr.w   lfx_idx_b
        clr.w   (%a0)
        moveq   #6, %d2
        cmp.l   %d3, %d2
        beq.s   1f
        moveq   #3, %d2
        cmp.l   %d4, %d2
        beq.s   2f
        moveq   #4, %d2
        cmp.l   %d4, %d2
        beq.s   2f
        jmp     0x4013783c              | TRIG: the stock start point into d2
2:      mvs.w   0x2c(%a4), %d2
        and.l   #0x7fff, %d2
        ori.l   #0x8000, %d2
        move.w  %d2, (%a0)
1:      moveq   #0, %d2
        jmp     0x40137854

lfx_eval_b:
        move.l  %d1, -(%sp)
        bsr.w   lfx_idx_b
        move.w  (%a0), %d1
        bpl.s   9f
        bsr.w   lfx_shift_b
        moveq   #5, %d0
        cmp.l   %d0, %d3
        bhi.s   2f
        moveq   #3, %d0
        cmp.l   %d4, %d0
        beq.s   9f
        moveq   #4, %d0
        cmp.l   %d4, %d0
        beq.s   9f
2:      move.l  %d2, 0x50(%a2)
        clr.w   (%a0)
9:      move.l  (%sp)+, %d1
        moveq   #50, %d0
        muls.l  %d0, %d2                | the stock: the phase x 50
        move.l  #2135039823, %d0
        rts

lfx_page_b:
        move.l  %d2, -(%sp)
        move.l  0x50(%a2), %d2
        bsr.w   lfx_idx_b
        move.w  (%a0), %d1
        bpl.s   1f
        bsr.w   lfx_shift_b
        mvs.b   0x2a(%a4), %d0          | WAVE (d3 holds DEST here)
        moveq   #5, %d1
        cmp.l   %d1, %d0
        bgt.s   2f
        moveq   #3, %d1
        cmp.l   %d4, %d1
        beq.s   1f
        moveq   #4, %d1
        cmp.l   %d4, %d1
        beq.s   1f
2:      move.l  %d2, 0x50(%a2)
        clr.w   (%a0)
1:      move.l  %d2, 0x70(%a2)
        move.l  (%sp)+, %d2
        rts
