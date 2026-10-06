| SPDX-License-Identifier: GPL-2.0-or-later
| core-dn2: the MIDI events of the Digitone II (OS 1.12). ColdFire V4;
| assemble with -mcpu=54455. Beside core.s, which is shared with the
| Digitakt mk1, the Digitone mk1 and the Digitakt II; these two events exist
| on the Digitone II only. Their addresses come from mod.json:
|
|   MIDI_BUILD   the sequencer's MIDI event builder for a MIDI-machine
|                track, (track, pattern, kit, step, event, &utm, &swing)
|                -> the voice event that wraps the MIDI event, or 0
|   VEL_SLOT     where the MIDI task keeps the note's velocity byte, as an
|                offset from its stack pointer at the ev_midi_note site
|
| ---- ev_midi_step: the sequencer built a MIDI-track trig --------------------
| At 0x400da0b2 in the step handler (vector 121, FUN_400d99de) and at
| 0x400d9230 in the pending-step pass (FUN_400d907a), the same two
| instructions: move.l d2,-(sp) ; jsr MIDI_BUILD(pc). Both run only for a
| track whose MIDI-machine bit (kit + 0x5cda) is set; the synth tracks take
| the other branch, which is left alone. The caller has pushed the builder's
| other six arguments; d2 = the track. core_midistep calls the builder with
| all seven, runs the handlers on the event it returns, and goes back with
| the stack as the replaced push left it (seven longs), d0 = the event, d2
| as it was.
|
| Handlers: f(void *vev, int track, int step, void *pattern, void *kit).
| vev is the voice event (type 3, scheduled by the caller after this); the
| MIDI event is the long at vev + 0x18 (docs/ADAPTING.md, "The hook bus on
| the Digitone II"). Interrupt level: keep them short, call no firmware that
| might block. Only .run code: this can come before the first ev_tick.
        .section .run, "ax"
        .globl  core_midistep
core_midistep:
        move.l  24(%sp), -(%sp)         | the builder's arguments 7..2, as pushed
        move.l  24(%sp), -(%sp)
        move.l  24(%sp), -(%sp)
        move.l  24(%sp), -(%sp)
        move.l  24(%sp), -(%sp)
        move.l  24(%sp), -(%sp)
        move.l  %d2, -(%sp)             | 1: the track (the push this replaced)
        jsr     MIDI_BUILD
        lea     28(%sp), %sp
        tst.l   %d0
        beq.s   3f
        move.l  %a2, -(%sp)
        move.l  %d0, -(%sp)             | (sp) event, 4 a2, 8 ret, 12 arg 2, 16 arg 3, 20 arg 4
        lea     ev_midi_step, %a2
1:      move.l  (%a2)+, %d0
        beq.s   2f
        move.l  16(%sp), -(%sp)         | kit (the caller's arg 3, at 16)
        move.l  16(%sp), -(%sp)         | pattern (arg 2, now at 16)
        move.l  28(%sp), -(%sp)         | step (arg 4, now at 28)
        move.l  %d2, -(%sp)             | track
        move.l  16(%sp), -(%sp)         | the event (now at 16)
        movea.l %d0, %a0
        jsr     (%a0)
        lea     20(%sp), %sp
        bra.s   1b
2:      move.l  (%sp)+, %d0
        movea.l (%sp)+, %a2
3:      lea     -4(%sp), %sp            | leave the replaced push on the
        move.l  4(%sp), (%sp)           | caller's stack: return address down
        move.l  %d2, 4(%sp)             | one long, the track in its place
        rts

| ---- ev_midi_note: the MIDI task is about to send a note-on ------------------
| At 0x4012b490 in the MIDI task (FUN_4012ac88), in its note loop (was:
| moveq #127,d1 ; and.l d0,d1 ; movea.l d1,a4, the range check's first half;
| the cmp.l d1,d0 and bne after it skip a note outside 0..127). Here
| d0 = the note (the stored note plus the track's and the global
| transpose), d5.b = the length index, VEL_SLOT(sp) = the velocity byte,
| d2 = the track, d7 = the channel (0-15), a2 = the MIDI event, a3 = the
| note's entry. What the handlers leave in the struct is what the task
| sends: the note-on, the retrigger off before it and the booked note-off
| all use it.
|
| Handlers: f(struct core_midi_note *n), the struct {int track, chan, note,
| vel, len; void *event;}: vel 0-127, len the length index (0-126, 127 =
| infinite), note: set it outside 0..127 and the task skips the note (no
| note-on, no note-off). Task level (the MIDI task, priority 8). Only .run
| code. Every register but d1 and a4 (which the replaced instructions set)
| is as the task left it, d0 = the note.
        .globl  core_midinote
core_midinote:
        lea     -36(%sp), %sp           | 0 a2, 4 a0, 8 a1, then the struct:
        move.l  %a2, (%sp)              | 12 track, 16 chan, 20 note, 24 vel,
        move.l  %a0, 4(%sp)             | 28 len, 32 event
        move.l  %a1, 8(%sp)
        move.l  %d2, 12(%sp)
        move.l  %d7, 16(%sp)
        move.l  %d0, 20(%sp)
        moveq   #0, %d1
        move.b  40+VEL_SLOT(%sp), %d1   | 36 of ours + the return address
        move.l  %d1, 24(%sp)
        moveq   #0, %d1
        move.b  %d5, %d1
        move.l  %d1, 28(%sp)
        move.l  %a2, 32(%sp)
        lea     ev_midi_note, %a2
1:      move.l  (%a2)+, %d0
        beq.s   2f
        pea     12(%sp)                 | the struct
        movea.l %d0, %a0
        jsr     (%a0)
        addq.l  #4, %sp
        bra.s   1b
2:      move.l  24(%sp), %d1
        move.b  %d1, 40+VEL_SLOT(%sp)
        move.b  31(%sp), %d5            | len's low byte; d5's others as they were
        move.l  20(%sp), %d0
        movea.l 8(%sp), %a1
        movea.l 4(%sp), %a0
        movea.l (%sp), %a2
        lea     36(%sp), %sp
        moveq   #127, %d1               | the instructions this replaced
        and.l   %d0, %d1
        movea.l %d1, %a4
        rts
