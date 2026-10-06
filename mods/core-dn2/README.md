# core (Digitone II)

The boot copier and the hook bus for Digitone II OS 1.12. Every set of
linkable (format 2) mods for the Digitone II needs it. On its own it changes
nothing the unit does. Experimental: version 0.1, checked in digikit's
emulator only, never run on a unit.

It builds the shared [../core/core.s](../core/core.s),
[settings.s](../core/settings.s) and [fast.s](../core/fast.s) and the
Digitakt II's [personalize.s](../core-dt2/personalize.s) with the Digitone
II's addresses (`mod.json`), and its own [midi.s](midi.s). The render hooks
(`render.s`) are not in it: the Digitone II renders its audio on the DSP.

- **At boot** (the OS entry's call at 0x40000538, which called 0x400019b0,
  as on the Digitakt II), `boot` copies the RAM image the linker built to
  the device's DDR area (0x47E00000), zeroes `.bss`, and goes on to the
  call it replaced. It leaves DTIM0 stopped, as stock does (`NO_DTIM0`).
- **The Digitakt II core's sites** become the same events, with the same
  prototypes ([docs/ADAPTING.md](../../docs/ADAPTING.md), "The hook bus"):

  | site | was | event |
  |---|---|---|
  | 0x4002e464 | `jsr 0x4011d5d4`: returns the view controller's byte +0x20 | `ev_tick` |
  | 0x4002e4ca | `jsr 0x4011d60a`: `drawAll(ctrl, bmp)`, the 128x64 frame | `ev_draw` |
  | 0x4002f20c | `jsr 0x4002c5c8`: `Brain::key(brain, event)` | `ev_key` |
  | 0x4002f256 | `jsr 0x4002c55c`: `Brain::enc(brain, event)` | `ev_enc` |
  | 0x4009ff88 | `movea.l (a2),a0; pea 2.w`, the SETTINGS builder 0x4009fbe0 going on at 0x4009ff8e | `ev_settings` |
  | 0x400987a2 | `movem.l $18(a7),d2-d6/a2-a6`, the end of the PERSONALIZE builder 0x40097eac | `ev_personalize` |

  `core_additem(menu, row)` uses `operator new` 0x40120544, `MenuItem`'s
  constructor 0x401173b8, `Menu::addItem` 0x40116bee and the
  `std::function` manager 0x4018de3e. Each site and routine was found by the
  Digitakt II 1.17's code with the addresses masked: each matched once, and
  each routine is the Digitakt II's whole, instruction for instruction.
- **Two MIDI events**, the Digitone II's own (`midi.s`):

  | site | was | event |
  |---|---|---|
  | 0x400da0b2 | `move.l d2,-(sp); jsr 0x400d8cbe(pc)`, in the step handler (vector 121) | `ev_midi_step` |
  | 0x400d9230 | the same, in the pending-step pass 0x400d907a | `ev_midi_step` |
  | 0x4012b490 | `moveq #127,d1; and.l d0,d1; movea.l d1,a4`, in the MIDI task's note loop | `ev_midi_note` |

  `ev_midi_step(vev, track, step, pattern, kit)` runs, at interrupt level,
  each time the sequencer builds the event for a MIDI-machine track's trig
  (0x400d8cbe); the MIDI event is the long at `vev + 0x18`.
  `ev_midi_note(n)` runs in the MIDI task for each note it is about to
  send, with `n` = `{int track, chan, note, vel, len; void *event;}`: what
  a handler leaves there is what goes out (the note-on, and the note-off
  the task books). A note set outside 0..127 is not sent. Subscribe only
  `.run` code to both: they can come before the first `ev_tick`.
- **`.fast` code** (`fast.s`): core declares the copy table `core_fast`,
  and its `ev_tick` handler copies every mod's `.fast` section into the SRAM
  tail (0x8000F980-0x80010000, 1,664 bytes) on the first tick, once.

Build it with the SDK (with Homebrew's `m68k-elf-binutils`, set
`ELEKLOADER_CROSS=m68k-elf-`):

```bash
python -m elekloader.sdk.build mods/core-dn2 --stock Digitone_II_OS1_12.syx      # out/core-0.1.elemod
python -m elekloader.lint mods/core-dn2/out/core-0.1.elemod --stock Digitone_II_OS1_12.syx
```

Checked in digikit's emulator (6 Oct 2026), by cold boots of the built
files: core alone boots to the same main screen as stock, and with
[examples/midi-hooktest-dn2](../../examples/midi-hooktest-dn2) the
sequencer's MIDI notes on DIN out are an octave up with their velocity set
to their step, at the same times as stock's. Not run on a unit.
