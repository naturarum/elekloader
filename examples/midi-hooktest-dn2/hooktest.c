/* SPDX-License-Identifier: GPL-2.0-or-later */
/* midi-hooktest: a test of core-dn2's two MIDI events (Digitone II 1.12).
 * Not a mod anyone would keep: it makes their effect easy to read on MIDI out.
 *
 * - ev_midi_step: each MIDI-machine trig the sequencer builds. The handler
 *   remembers (MIDI event, step) in a small ring.
 * - ev_midi_note: each note the MIDI task is about to send. For a note from
 *   the sequencer (the MIDI event's source word, +0x34, is 1), the handler
 *   raises it an octave and, when the ring has its event, sets its velocity
 *   to the trig's step + 1. Live notes (source 2) are left alone: their
 *   note-off comes from the key release, which this event does not see.
 *
 * So a trig on step 1 (index 0) that stock sends as 90 3C 64 goes out as
 * 90 48 01, on step 5 as 90 48 05, and so on; the note-off the task books is
 * the transposed note (90 48 00). The counters are for reading RAM.
 */
struct core_midi_note {
    int track, chan, note, vel, len;
    void *event;
};

#define MEV_SOURCE(ev) (*(volatile int *)((char *)(ev) + 0x34))   /* 1 sequencer, 2 live */
#define VEV_MIDI(vev)  (*(void **)((char *)(vev) + 0x18))         /* voice event -> MIDI event */

#define RING 16
static struct { void *ev; int step; } seen[RING];
static unsigned int seen_n;
unsigned int hooktest_steps;     /* ev_midi_step calls */
unsigned int hooktest_notes;     /* sequencer notes changed */
unsigned int hooktest_tagged;    /* of those, velocity set from the step */

void hooktest_step(void *vev, int track, int step, void *pattern, void *kit)
{
    (void)track; (void)pattern; (void)kit;
    unsigned int i = seen_n % RING;
    seen[i].ev = VEV_MIDI(vev);
    seen[i].step = step;
    seen_n++;
    hooktest_steps++;
}

void hooktest_note(struct core_midi_note *n)
{
    unsigned int k;
    if (MEV_SOURCE(n->event) != 1)
        return;
    hooktest_notes++;
    n->note += 12;
    for (k = 1; k <= RING && k <= seen_n; k++) {     /* newest first */
        unsigned int i = (seen_n - k) % RING;
        if (seen[i].ev == n->event) {
            n->vel = (seen[i].step & 0x7f) + 1;
            if (n->vel > 127)
                n->vel = 127;
            hooktest_tagged++;
            break;
        }
    }
}
