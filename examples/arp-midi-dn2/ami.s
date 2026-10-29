| SPDX-License-Identifier: GPL-2.0-or-later
| arp-midi-dn2 0.1 (Digitone II OS 1.12): the arpeggiator on MIDI-machine tracks. Stock keeps the arp off them twice:
| the UI (the ARP key, the ARP menu's auto-close, the ARP LED) and the engine (the sequencer sends a MIDI track's trigs
| down the MIDI builder, live keys go straight to the MIDI task, and the arp's notes go only to the voice allocator).
| This mod routes a MIDI track whose arp is on through the stock arp, and turns its notes into MIDI events on the list
| the voice interrupt already posts to the MIDI task, so they leave as a stock MIDI track's trigs do, on its channel.
| The trig's MIDI p-locks (CC, pitch bend, aftertouch, program) are not sent for an arp-on MIDI track; arp off, the
| track is stock. SHUF, RAND and TM (arp-modes) play there as on a synth track. Design and every address: dn2-cfw
| notes/T2.2-arp-midi-v2.md (option 2) and notes/BA05-arp-midi.md; scripts/ba05_make_midi.py checks every site against
| the stock bytes. Our own code. ColdFire V4; assemble with -mcpu=54455.
|
|   0x4005f9c2  the ARP key: bne.w (a MIDI track returns)            -> nop nop
|   0x400187f6  the ARP menu's lambda: beq.s (not MIDI: keep it open) -> bra.s (always)
|   0x40060dea  the ARP LED: bne.s (a MIDI track: off)                -> nop
|   0x400269e6  voice interrupt, note-on: jsr the voice allocator     -> jsr ami_sink
|   0x40026d86  voice interrupt, note-off: jsr the voice allocator    -> jsr ami_off
|   0x40121f2a  live note-on: jsr FUN_40027c18 ("not MIDI")            -> jsr ami_liveon
|   0x40121bcc  live note-off: jsr FUN_40027c18                         -> jsr ami_liveoff
|   0x400da036  the sequencer's step: movea.l 0x42c5b9ac,a0           -> jsr ami_seq
|   0x400da040  then mvs.w (0x5cda,a0),d0; btst d2,d0 (the MIDI split) -> tst.b d0; nop; nop (the bne.s after kept)
|   0x400d91bc  the pending step: movea.l 0x42c5b9ac,a0               -> jsr ami_pre
|   0x400d91c2  then mvs.w (0x5cda,a0),d1; btst d2,d1                 -> movea.l 0x42c5b9ac,a0 (the bne.s after kept)

        .set MIDIMASK_ISR, 0x8000537c   | the interrupt's copy of the kit's MIDI-track mask (u16, bit t)
        .set ISR_KIT,      0x800052a0   | the kit the interrupt uses; track t's sound record at kit + 0x34 + t x 0x48b
        .set SEQ_KIT,      0x42c5b9ac   | the sequencer's kit
        .set ARP_MODE,     0x193        | 0x34 + 0x15f: the arp's MODE in the kit, track t's at + t x 0x48b
        .set SOUND_STRIDE, 0x48b
        .set ARP_SOUND,    0x4058f8d8   | the arp's sound record per track (set at 0x4002683c)
        .set MEV_FREE,     0x4460f4b8   | the MIDI events' free list
        .set MEV_ALLOC,    0x4012a6e8   | takes one; no empty check, so MEV_FREE is tested first
        .set VOICE_ALLOC,  0x4002a6a2
        .set IS_SYNTH,     0x40027c18   | "track t is not MIDI" (reads MIDIMASK_ISR)
        .set LIVE_TO_ISR,  0x4013801c   | posts a live key's event to the voice interrupt

        .section .run, "ax"
        .globl  ami_sink
        .globl  ami_off
        .globl  ami_liveon
        .globl  ami_liveoff
        .globl  ami_seq
        .globl  ami_pre

| ami_sink: the voice allocator's call in the interrupt's note-on loop (0x400269e6). Its 9 arguments as pushed at
| 0x400269b4..0x400269e4: 4(sp) track, 8(sp) raw note, 12(sp) ev[1], 16(sp) 0xf, 20(sp) &fp-72, 24(sp) ev[5],
| 28(sp) ev[0x17], 32(sp) flags & 0x80000, 36(sp) sound. The interrupt's frame (fp, from 0x40025e36): fp-36 the
| event, fp-44 its time bucket (+4 its time), fp-48 the pass's end, fp-180 the MIDI list's head, a5 its tail; a2 the
| note record (+2 note, +3 velocity, +4 length). A synth track: the stock allocator. A MIDI track: one MIDI note event
| appended to the list as the type-3 path does (0x400262ca..0x4002632a), and no voice (d0 = d1 = 0).
ami_sink:
        move.l  %sp@(4), %d0
        moveq   #16, %d1
        cmp.l   %d1, %d0
        bcc.s   9f                      | track 16: stock
        mvz.w   MIDIMASK_ISR, %d1
        btst    %d0, %d1
        bne.s   1f
9:      jmp     VOICE_ALLOC             | a synth track: stock
1:      lea     %sp@(-16), %sp
        movem.l %d2-%d3/%a3-%a4, %sp@
        tst.l   MEV_FREE
        beq.w   8f                      | no MIDI event free: the note is dropped, no voice
        jsr     MEV_ALLOC               | d0 = the event, [0], [1] (locks) and [0x17] cleared
        movea.l %d0, %a3
        movea.l %fp@(-36), %a1          | the voice event
        movea.l %a1@(44), %a4           | ev[0xb], the track data (0 for live events)
        move.l  %sp@(16+4), %d2         | the track
        move.l  %d2, %a3@(8)            | [2] track
        moveq   #1, %d1
        move.l  %d1, %a3@(12)           | [3] 1: note on
        move.l  #0x80, %d1
        move.l  %d1, %a3@(16)           | [4] bit 7: play the notes (the gate at 0x4012b2d8)
        clr.l   %a3@(20)                | [5] pool: unused with [6] < 0
        moveq   #-1, %d1
        move.l  %d1, %a3@(24)           | [6] -1: the note inline at +0x24
        moveq   #1, %d1
        cmp.l   %a1@(20), %d1           | ev[5], the source: 1 the sequencer, 2 live
        beq.s   2f
        moveq   #2, %d1
2:      move.l  %d1, %a3@(52)           | [0xd]: 1 the mutes apply, 2 live (as FUN_4012bb90)
        move.w  %a2@, %a3@(36)          | +0x24, +0x25 (not read by the MIDI task)
        move.b  %a2@(2), %a3@(38)       | +0x26 the RAW note: the MIDI task adds the transposes
        move.b  %a2@(3), %d1            | velocity; never negative with [0xb] = 0 (live)
        bpl.s   3f
        moveq   #100, %d1
        cmpa.l  #0, %a4
        beq.s   3f
        move.b  %a4@(0x480), %d1        | the track's default velocity
        bpl.s   3f
        moveq   #100, %d1
3:      move.b  %d1, %a3@(39)           | +0x27 velocity
        move.l  %a1@(56), %d1           | the length
        andi.l  #0x80000, %d1           | a note of the arp's?
        beq.s   5f
        lea     ARP_SOUND, %a0
        movea.l %a0@(0,%d2.l*4), %a0    | the arp's sound record for the track
        mvs.b   %a0@(0x162), %d1        | N.LEN (an index into 0x40288048)
        bmi.s   4f
        cmpi.l  #126, %d1
        bhi.s   4f                      | 127, INF: one arp period instead
        lea     ami_len, %a0
        move.b  %a0@(0,%d1.l), %d1      | the first trig length not shorter
        bra.s   6f
4:      lea     ARP_SOUND, %a0
        movea.l %a0@(0,%d2.l*4), %a0
        mvs.b   %a0@(0x160), %d1        | SPEED
        cmpi.l  #22, %d1
        bls.s   41f
        moveq   #22, %d1
41:     lea     ami_period, %a0
        move.b  %a0@(0,%d1.l), %d1      | one arp period
        bra.s   6f
5:      move.b  %a2@(4), %d1            | not the arp's: the note's own length
        bpl.s   6f
        cmpa.l  #0, %a4
        bne.s   6f                      | negative with track data: the MIDI task resolves it
        moveq   #0x7f, %d1              | live: held until the key's note-off
6:      move.b  %d1, %a3@(40)           | +0x28 length
        clr.b   %a3@(41)                | +0x29 micro-timing 0
        move.l  %a4, %a3@(44)           | [0xb] track data
        movea.l %fp@(-44), %a0          | the time bucket
        move.l  %fp@(-48), %d1          | the pass's end
        tst.l   %a0@
        bne.s   7f                      | a bucket played at once (a live key's): its time field isn't the time
        move.l  %a0@(4), %d1            | a timed bucket: its time, as the type-3 path at 0x4002629e
7:      move.l  %d1, %a3@(48)           | [0xc] time: the booked note-off counts from it
        move.l  %fp@(-48), %d1          | [0xe] as the type-3 path (unused for an inline note)
        tst.l   %a0@
        bne.s   73f
        move.l  %a1@(8), %d1
73:     move.l  %d1, %a3@(56)
        clr.l   %a3@(60)                | [0xf]
        clr.l   %a3@(64)                | [0x10]
        tst.l   %fp@(-180)              | appended as at 0x400262ca..0x4002632a
        bne.s   71f
        move.l  %a3, %fp@(-180)
        bra.s   72f
71:     move.l  %a3, %a5@(92)
72:     movea.l %a3, %a5                | a5, the interrupt's list tail
8:      movem.l %sp@, %d2-%d3/%a3-%a4
        lea     %sp@(16), %sp
        moveq   #0, %d0                 | no voice to steal
        moveq   #0, %d1                 | no voice to start
        rts

| ami_off: the allocator's call in the note-off loop (0x40026d86). A MIDI track owns no voice: no allocator, so a
| stale voice record can't be released for it. The caller reloads d0 (0x40026d90) and tests d1 (0x40026d96).
ami_off:
        move.l  %sp@(4), %d0
        moveq   #16, %d1
        cmp.l   %d1, %d0
        bcc.s   1f
        mvz.w   MIDIMASK_ISR, %d1
        btst    %d0, %d1
        beq.s   1f
        moveq   #0, %d0
        moveq   #0, %d1
        rts
1:      jmp     VOICE_ALLOC

| ami_liveon: FUN_40027c18 at the live note-on (0x40121f2a, FUN_40121db2). A MIDI track with its arp on reads as
| "not MIDI", so the key goes to the voice interrupt (FUN_4013801c) and into the arp.
ami_liveon:
        move.l  %sp@(4), %d0
        moveq   #16, %d1
        cmp.l   %d1, %d0
        bcc.s   9f
        mvz.w   MIDIMASK_ISR, %d1
        btst    %d0, %d1
        beq.s   9f
        mulu.w  #SOUND_STRIDE, %d0
        movea.l ISR_KIT, %a0
        adda.l  %d0, %a0
        tst.b   %a0@(ARP_MODE)
        beq.s   9f                      | arp off: stock (to the MIDI task)
        moveq   #1, %d0
        rts
9:      jmp     IS_SYNTH

| ami_liveoff: FUN_40027c18 at the live note-off (0x40121bcc, FUN_40121b0a). A MIDI track's key release goes to both
| engines, whatever the arp is now: the voice interrupt (the key leaves the arp's held set; nothing if it isn't
| there) and, returning 0, the MIDI task as stock (it sends 9n kk 00 only for a note sounding). d6 is the caller's
| event (a6 - 0x40, built at 0x40121ba2..0x40121bc8).
ami_liveoff:
        move.l  %sp@(4), %d0
        moveq   #16, %d1
        cmp.l   %d1, %d0
        bcc.s   9f
        mvz.w   MIDIMASK_ISR, %d1
        btst    %d0, %d1
        beq.s   9f
        move.l  %d6, -(%sp)
        jsr     LIVE_TO_ISR
        addq.l  #4, %sp
        moveq   #0, %d0
        rts
9:      jmp     IS_SYNTH

| ami_seq: the sequencer's MIDI split (FUN_400d99de, 0x400da036; the stock clr.l 96(sp) runs after it, then tst.b d0
| and the stock bne.s 0x400da09e). d0.b nonzero: the MIDI builder; zero: the synth path (the arp's step sync and the
| voice event, FUN_400d87b8), which an arp-on MIDI track takes so its trigs feed the arp. d2 the track, d6 the kit of
| the trig's sound; d0, d1, a0, a1 are dead at both targets.
ami_seq:
        movea.l SEQ_KIT, %a0
        mvs.w   %a0@(0x5cda), %d0
        btst    %d2, %d0
        beq.s   8f                      | a synth track
        move.l  %d2, %d0
        mulu.w  #SOUND_STRIDE, %d0
        movea.l %d6, %a0
        adda.l  %d0, %a0
        tst.b   %a0@(ARP_MODE)
        seq     %d0                     | arp off: 0xff, the MIDI builder; on: 0, the synth path
        rts
8:      moveq   #0, %d0
        rts

| ami_pre: the pending step's MIDI split (FUN_400d907a, 0x400d91bc; then the stock bne.s 0x400d91f0 after a reload of
| a0). Returns Z clear for the MIDI builder. d0 (the step) is kept; d1 and a1 are dead.
ami_pre:
        movea.l %d0, %a1                | keep d0
        movea.l SEQ_KIT, %a0
        mvs.w   %a0@(0x5cda), %d1
        btst    %d2, %d1
        beq.s   2f                      | a synth track
        move.l  %d2, %d0
        mulu.w  #SOUND_STRIDE, %d0
        adda.l  %d0, %a0
        moveq   #1, %d1                 | MIDI, arp off: the MIDI builder
        tst.b   %a0@(ARP_MODE)
        beq.s   3f
2:      moveq   #0, %d1                 | a synth track, or MIDI with its arp on: the synth path
3:      move.l  %a1, %d0
        tst.l   %d1                     | Z for the caller's bne.s
        rts

| N.LEN (the index into the arp's lengths, 0x40288048) -> the first trig length (0x401d8ce8) not shorter; 127 (INF)
| is not looked up. Made by scripts/ba05_make_midi.py from the two stock tables.
ami_len:
        .byte 0x00,0x00,0x01,0x01,0x02,0x02,0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x07
        .byte 0x08,0x08,0x09,0x09,0x0a,0x0a,0x0b,0x0b,0x0c,0x0c,0x0d,0x0d,0x0e,0x0e,0x0f,0x0f
        .byte 0x10,0x10,0x11,0x11,0x12,0x12,0x13,0x13,0x14,0x14,0x15,0x15,0x16,0x16,0x17,0x17
        .byte 0x18,0x18,0x19,0x19,0x1a,0x1a,0x1b,0x1b,0x1c,0x1c,0x1d,0x1d,0x1e,0x1e,0x1f,0x20
        .byte 0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30
        .byte 0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40
        .byte 0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x52,0x56
        .byte 0x5a,0x5e,0x60,0x62,0x64,0x66,0x68,0x6a,0x6c,0x6e,0x6f,0x72,0x76,0x7a,0x7e,0x7f
| SPEED (0..22) -> the first trig length not shorter than one arp period: an INF note lasts until the next one.
ami_period:
        .byte 0x4e,0x46,0x3e,0x36,0x34,0x2e,0x28,0x26,0x24,0x1e,0x18,0x16,0x14,0x0e,0x0b,0x0a
        .byte 0x09,0x06,0x05,0x04,0x02,0x02,0x01
        .even
