# Adding a device

Everything elekloader knows about a product is in one profile,
`elekloader/devices/<device>.py`, registered in `devices/__init__.py`
(`_all()`). The Digitakt mk1 profile is the example. The Digitone mk1's
(`digitone_mk1.py`) is a second device of the same file family, with its own
core. The Octatrack's (`octatrack.py`) is a second file family, with a core
and a hook bus of its own.

| field | what it is | how it was found for the Digitakt mk1 |
|---|---|---|
| `releases` | each supported stock OS: its `.syx` sha256, and its main OS section's sha256 and length | hash the file, and the section depacked |
| `sysex_id` | the device byte of its OS messages | the framing messages (`0x0A`) |
| `main_section`, `main_load` | which container section is the main OS, and where it runs | the section table (id 3, `0x40000400`) |
| `stage` | where the bootloader stages the main OS before unpacking it in place | the bootstrap's loader (`0x800005fe`: read to `0x40200000`, unpack to `0x40000400`) |
| `flash_at`, `flash_limit` | where the container sits in flash and must end | the bootstrap (`0x80000` .. `0x380000`) |
| `more_stages` | further places another loader stages the main OS and unpacks it in place from; verify checks the gap at each | none on the mk1 (the Digitone II's section 4 stages at `0x40200000`) |
| `receive_max` | the OS's upgrade receive buffer: the decoded stream, in whole 101-byte chunks, must fit | none on the mk1 (the Digitone II's is 2 MiB) |
| `packer` | the main OS's packer: `'aplib'` (`codec/aplib.py`) or `'dense'` (`codec/dense.py`, with its in-place floor at the lowest stage) | `'aplib'` |
| `trailer` | how the file is sealed: `None`, or `'hmac'` | mk1 has none |
| `hmac_key_from` | for `'hmac'`: the section and seed string the key is derived from (never the key itself) | none on the mk1 |
| `isa` | the main CPU's instruction decoder, for the boundary checks | ColdFire (`isa/coldfire.py`) |
| `areas` | memory that is free at run time: where a mod's regions may lie | the research on what the OS never touches |
| `ddr`, `sram_code`, `fast_table` | where the linker puts mods' code and data, fast code, and the table that copies it | the areas above |
| `recovery` | how to get back to stock, shown to the user | FUNC at power-on |
| `container`, `version_len` | the file family (`'ele3'`, `'elek'`) and the version field's length | the container header |
| `protected` | main OS ranges no mod may change, with the reason | none on the mk1 |
| `relocatable` | data inside a protected range that the OS reaches only through the listed 4-byte operands and never writes, so a mod may serve its own copy through them (sdk.octabam does, for a poke there) | none on the mk1 |
| `blob_max` | a cap on the appended blob, if the DDR areas do not give one | none on the mk1 |
| `image_free` | zero runs inside the main OS that `fixed` code may take (sdk.build) | none on the mk1 |
| `toolchain` | the compiler, assembler and flags the SDK uses | the CFW's build |

## The Digitakt II (1.17)

Experimental. Whole builds and the core (mods/core-dt2) boot in digikit's
emulator, and a build of core with examples/perform-direct works on a unit
(4 Oct 2026): sent with Elektron Transfer over USB (the in-OS upgrade), it
was accepted and booted, the unit reported the build's own version (PD10),
and the key swap and a PERSONALIZE row (in an earlier version of the mod)
worked. digikit (https://github.com/m-dwyer/digikit,
docs/findings/01-container-and-patching.md) mapped 1.15C and 1.16. Each fact
below was found again in 1.17.

- **Files.** The Digitakt mk1's family (ELE3, the same SysEx transport),
  device byte `0x14`. It has six sections: 5, 2 (the bootstrap, run at
  `0x80000400`; its `dest` is a version, `0x0201`), 3 (MAIN OS), 4 (the
  updater), 7 (the SHARC DSP's program) and 8.
- **Sealed.** After the last section's 16-byte padding comes a 32-byte
  HMAC-SHA256 of everything before it, inside the preamble's length.
  - The bootstrap checks it, and the content checksum, before it flashes;
    the OS checks it on an upgrade. A file that fails is refused with
    `Checksum failed` or `UPGRADE ABORTED`, and nothing is written.
  - The key: in section 2, the seed `"Master Overdrive"`, a NUL and a
    32-byte constant C; the key is C ^ sha256(seed) ^ sha256(reversed seed).
    `syx.seal_key` reads it from the user's stock file, so it is never
    stored here.
  - From its own main OS stream, the writer reproduces the stock file byte
    for byte, seal included.
- **Staging.** The bootstrap's loader (`0x80000596`) reads section 3 from
  flash, at its container offset + `0x80000`, to `0x40400000`, and unpacks
  it from there to `0x40000400` with its depacker (`0x80000432`). The mk1
  stages at `0x40200000`; this image, 3.27 MB depacked, runs past that.
  Stock's in-place gap is 2,065,240 bytes.
- **The unit's own depacker takes elekloader's stream.** Run in Unicorn
  through digikit's harness (`emu/oracle.py`, with the 1.17 entry), the
  bootstrap's depacker yields the stock main OS from both Elektron's
  stream and elekloader's repack of it (1,385,586 bytes against
  1,147,584).
- **Flash.** The container starts at `0x80000`: the bootstrap's read above,
  and digikit's map of the OS's own reads. The OS keeps a store at
  `0x380000`-`0x400000`, with the mk1's code at its own addresses: it reads
  a 0x14-byte header at `0x380000`, and erases the sectors at `0x380000` and
  `0x3c0000` (26 and 12 call sites, from `0x400ca504`). So `flash_limit` is
  `0x380000`: 3 MB for the container, of which stock takes 1.48 MB. The
  bootstrap's receive path has no size check of its own, so this limit is
  elekloader's alone.
- **Checked in an emulator** (digikit's, `emu.checkpoint` from a cold
  boot, then `tools/emucheck.py` to 600M instructions; 4 Oct 2026):
  - Two elekloader builds, a repack of stock 1.17 (version `DT01`) and a
    whole build with one data site (`DT02`), pass every stage as stock does:
    the main screen, six tasks, the +Drive formatted, the same instruction
    count. The repack's screen at 650M is identical to stock's.
  - digikit's extractor reads both, and its checker passes their content
    checksum, seal, framing count and every packet checksum.
  - Not checked in the emulator: the bootstrap's own flash path (it serves
    the container to the OS's flash reads and boots the main OS directly).
    The unit above accepted a sealed build.
- **What writes the flash** (tests/test_digitakt2.py checks each against
  the stock file). The OS's SPI NOR driver writes through 0x40137cbe and
  erases through 0x40137d3a/0x40137da4 (256 KB) and 0x401373e6 (64 KB). Its
  only callers:
  - the settings store, 0x380000-0x400000 (above);
  - the in-OS upgrade, 0x4013ac54 (the route Transfer takes over USB): it
    runs the validator 0x400d9fb0 (content checksum, the build floor,
    the seal), then erases from 0x80000 block by block to cover the
    container and writes it there. A container that ends below 0x380000
    cannot make it erase past it: 0x380000 - 0x80000 is a whole number of
    256 KB units;
  - the factory console's upload (0x400caff0, "READY FOR OS" over the debug
    UART), which writes a container to 0x80000 or another image to
    0x40000-0x80000.
  None of them writes anything from the main OS image: unlike the Analog
  Rytm's, it holds no copy of the bootstrap to re-flash, and needs no
  `protected` range. The bootstrap itself is section 2, which every build
  keeps stock (version 0x0201: the bootstrap's own upgrade only runs for a
  greater one). The bootstrap's MIDI upgrade (the STARTUP menu) writes the
  container to 0x80000 too, and checks the content checksum and the seal
  first.
- **The version a unit reports** is its container's. The OS caches the
  container's first 0x20 bytes from flash 0x80000 (0x4013641c) and reads
  the build and version fields from there (0x40124978, the answer to
  Transfer's version request; an exception report); there is no version
  string in the main OS. So a build's 4-character version field is what
  the unit shows (a unit reported PD10 for a build so named), and the
  loader's default (`2.0a`) differs from stock's.
- **Recovery.** The startup menu (FUNC at power-on), TRIG 4 for OS UPGRADE,
  over MIDI only (Elektron's readme). It is in the bootstrap, which no build
  changes, and checks only the content checksum and the seal.
- **At reset** (`0x400004e8`), the OS:
  - copies `0x40312000-0x40318e80` to SRAM at `0x80000000` and
    `0x40318e80-0x4031ff60` to `0x80008000`, and zeroes the rest of each
    32 KB half;
  - clears DDR `0x40312000-0x47E28470`;
  - runs its stack down from `0x48000000`.
  The mk1's reset code, with the II's addresses.
- **Free memory, and the core** (mods/core-dt2):
  - There is no sample pool in its DDR to take from: samples go to the DSP
    over FlexBus (0x8C000000, digikit). The reset clears 0x40312000-
    0x47E28470 for the OS's static `.bss`, almost all of the 128 MB.
  - Above the clear, the OS reaches its DSP buffers only through the
    uncached alias (+0x08000000): 0x4FE30000-0x4FE7AE80, so 0x47E30000-
    0x47E7AE80. The boot stack runs down from 0x48000000.
  - Between them, 0x47E7B000-0x48000000: no decoded instruction (digikit's
    `tools/refscan.py`, 96.75% coverage) names it or its alias (the only
    hit, 0x47efffff, is the high word of the double FLT_MAX); it is zero in
    every rung of a cold boot; and a write watch over all of it, cached and
    uncached, saw no write from 400M to 808M instructions of a session
    with 37 key events. The core takes 0x47F00000-0x47F40000 (256 KB) from
    the middle, 520 KB clear of the buffers and 768 KB of the stack.
  - SRAM: the reset copies the image's tail into each 32 KB half and zeroes
    the rest. Past the copies, 0x80006E80-0x80008000 and 0x8000F100-
    0x80010000 are named by no decoded instruction. They are zeroed after
    the boot copier runs, so they are free at run time only, as on the mk1.
    A write watch over both saw no write from 400M to 781M instructions of
    a session with 24 key presses. `.fast` code goes to the tail: core-dt2
    declares `core_fast` and copies it on the first `ev_tick`.
  - The core does not start DTIM0, unlike the mk1's (`NO_DTIM0` in
    core.s). Nothing before the OS starts it on the II either, and ten OS
    sites read its counter: with it running, a cold boot with core alone
    jumped to 0 at 553M instructions in digikit's emucheck.
  - The core's events: `ev_tick`, `ev_draw`, `ev_key`, `ev_enc`,
    `ev_settings`, and the II's own `ev_personalize` (SETTINGS >
    PERSONALIZE, whose builder makes its rows with the same generic
    `MenuItem`). mods/core-dt2/README.md lists the sites and routines.
  - Checked in the emulator (cold boot, then emucheck to 600M): core 1.0
    alone passes every stage, with its screen at 650M identical to stock's;
    with a mod that inverts an 8x8 block on `ev_draw`, the screen differs
    from stock in exactly those 64 pixels. Each event was driven with a
    test mod: key and encoder counts, a SETTINGS row, and
    a PERSONALIZE checkbox row. examples/perform-direct (a key swap) works
    on a unit, with core's other events registered but not used there.
  - Not covered: anything a session in the emulator does not reach (no
    DSP, no samples, an empty +Drive) and the unit run did not use.

## The Digitone II (1.12)

Experimental: whole builds (format-1 mods), and from core-dn2 0.1
(mods/core-dn2) linkable mods, checked in digikit's emulator only. Not yet
run on a unit. Each fact below was
found again in 1.12 with the Digitakt II's as the map, and
tests/test_digitone2.py checks each one against the stock file.

- **Files.** The Digitakt II's family, device byte `0x15`, the same six
  sections: 5, 2 (the bootstrap, at `0x80000400`; its `dest` is the
  version `0x0201`), 3 (MAIN OS, 3,196,288 bytes, to `0x4030C980`), 4 (a
  factory board test that can also boot the OS, below), 7 (the SHARC's
  program) and 8 (firmware the OS sends to a USB accessory). From its own
  main OS stream, the writer reproduces the stock file byte for byte.
- **Sealed** as the Digitakt II's are, with the seed `"Multiplier"` (the
  bootstrap derives the key at `0x80005da4` and checks the trailer at
  `0x80005e3a`).
- **Two loaders, so two stages** (`stage`, `more_stages`):
  - the bootstrap's (`0x80000596`) reads section 3 from flash, at its
    offset + `0x80000`, to `0x40400000`, and depacks it to `0x40000400`
    (`0x80000432`). Stock's in-place gap: 2,128,334 bytes;
  - section 4's (`0x8000357c`) reads it to `0x40200000` and depacks it in
    place with its copy of the same depacker (`0x80005728`). The bootstrap
    runs section 4 instead (`0x80000eee`) when boot flag bit 6 is set, and
    only `0x80000b9a` sets it, when the power-on key read from the panel
    (`0x80001276`) times out. Stock's gap there is 31,182 bytes;
    elektron-firmware-tool's default level gives -39,318, and under Unicorn
    the unit's code then depacks wrong from `0x40303180` and never returns.
    So verify needs a gap > 0 at both stages.
- **The receive buffer** (`receive_max`). The in-OS upgrade (Transfer's
  route, over USB or MIDI) decodes the file into the static buffer at
  `0x4038bee0` (`0x40122574`), one 101-byte chunk per message
  (`0x4012257c`), with no bound on the message count. The next object the
  code names is at `0x4058bee0`. So the decoded stream must fit 2 MiB, or
  the upgrade overruns RAM. Stock fills 1,886,074 bytes of it. This is
  tighter than the flash: the settings records are at `0x380000` and
  `0x3c0000` (26 and 12 call sites), so `flash_limit` is `0x380000`.
- **The packer** (`packer='dense'`). Packed by `codec/aplib.py`, the
  stock image's section is 1,392,294 bytes, 260,946 more than Elektron's
  1,131,348: the container, 2,146,960 bytes, overruns the buffer.
  `codec/dense.py` packs it to 1,155,260, with a gap of 55,094 at
  `0x40200000` and 187,141 bytes spare in the buffer. When a build's image
  grows with data that packs well, dense keeps the gap at `0x40200000`
  with literals (its floor). The two limits together cap how far the image
  can grow: about 164 KB of data that does not pack (168,000 random bytes
  fit, 168,320 do not), about 236 KB of zeros.
- **Checked with the unit's own code** (digikit's
  `tools/acceptoracle.py`, under Unicorn, 5 Oct 2026):
  - a no-mod build (version DN01): section 4's loader and the
    bootstrap's boot path both give the stock image and enter the main OS
    at `0x400004e8`; the bootstrap's verify-and-commit and the OS's
    upgrade validator pass;
  - the stock image with 120 KB of zeros appended, packed with the floor
    (gap exactly 1 at `0x40200000`): both loaders give the intended image;
  - the same image packed without the floor (gap -67,685 at
    `0x40200000`, refused by verify): the bootstrap's path gives the right
    image, but section 4's loader gets 1,623 bytes wrong from `0x4031a7b2`
    and never reaches the main OS. The second stage's check refuses
    exactly what the unit would get wrong.
- **Checked in an emulator** (digikit's, `emu.checkpoint` from a cold boot,
  then `tools/emucheck.py` to 600M instructions; 5 Oct 2026): the no-mod
  build passes every stage as stock does, with the same task creations at
  the same instruction counts, the main screen first at instruction
  741,578,400, and a main screen byte-identical to stock's.
- **What writes the flash** (tests/test_digitone2.py checks each against
  the stock file). The SPI NOR driver's program, erase and protection
  entries (`0x40134a28`-`0x4013621c`) are named only by the settings store
  (`0x400cccc6`-`0x400cd21e`), the factory console (`0x400cd5f2`-
  `0x400cebd2`, only in maintenance mode), the migration that erases a
  `COKi` block at `0x400000`, `0x800000` or `0xc00000` (`0x400f2022`), and
  the in-OS upgrade writer (`0x40139104`), which runs the validator
  `0x400dbdb0` first and then erases and writes from `0x80000` with no
  length clamp. The console's `#FULL_UPGRADE` can rewrite the bootstrap
  at `0x40000` (the Digitakt II section above finds no such path there); it is
  dormant unless the unit starts in maintenance mode.
- **Protected** (`protected`): the panel MCU updater (`0x400dba28`-
  `0x400dbdae`, run on every normal boot; it reprograms the front panel
  when its firmware is older than the word at `0x402f7d44`), the panel
  firmware it sends (`0x402f7d40`-`0x402fb494`: the updater's own
  `move.l #0x402fb494,d3 ; subi.l #0x402f7d40,d3`), the NOR driver, and
  the in-OS writer. The STARTUP menu needs the panel to answer.
- **Recovery.** The startup menu (FUNC at power-on), TRIG 4 for OS
  UPGRADE, then the stock `.syx` over 5-pin DIN MIDI: the bootstrap has
  no USB code. It checks the content checksum and the seal before it
  writes. A unit went through it on 5 Oct 2026 with stock 1.12.
- **At reset** (`0x400004e8`), the OS:
  - copies `0x402fd000-0x40305000` to SRAM at `0x80000000` and
    `0x40305000-0x4030c980` to `0x80008000` (both all zeros in the
    image), and zeroes `0x8000f980-0x80010000`;
  - clears DDR `0x402fd000-0x466b84d0`, from inside the image's end, so a
    blob appended there is cleared too unless something copies it first;
  - runs its stack down from `0x48000000`.
- **Free memory** (`areas`, `ddr`, `sram_code`):
  - DDR `0x46800000-0x47F00000`. Above the clear, the code names only the
    DMA buffers, all through the uncached alias, up to `0x4e709340`
    (`0x46709340` cached). Over `0x466b84d0-0x48000000` and its alias,
    an objdump sweep and Ghidra's references find no other address but the
    clear's own end, only constants (the double FLT_MAX's high word,
    `GNUC`, 2^31 as a float, a decimal limit). In digikit's emulator, no page of the area or its
    alias is ever touched (digikit maps a page on its first access), in
    any rung of a cold boot to 400M instructions or in a session from
    400M to 800M with 32 key presses and 7 encoder turns, and write
    watches over all of it saw nothing in either run.
  - SRAM `0x8000f980-0x80010000` (1,664 bytes): past the highest array
    (17 rows of 404 bytes from `0x8000dea4`, to `0x8000f978`), named by no
    decoded instruction, and written only by the reset's zeroing in the
    same runs. The first 32 KB has no free block: code uses it up to
    `0x80008000`.
  The linker's `ddr` is the top megabyte of the DDR area,
  `0x47E00000-0x47F00000`; `areas['ddr']` keeps the rest
  (`0x46800000-0x47E00000`) for regions mods claim, so a region cannot
  overlap the linker's memory. `sram_code` is the SRAM tail.
- **The core** (mods/core-dn2 0.1). The Digitakt II core's boot site and
  six event sites, found again in 1.12 by their code with the addresses
  masked (each matched once; every routine core calls is the Digitakt
  II's whole, instruction for instruction): boot `0x40000538`, `ev_tick`
  `0x4002e464`, `ev_draw` `0x4002e4ca`, `ev_key` `0x4002f20c`, `ev_enc`
  `0x4002f256`, `ev_settings` `0x4009ff88`, `ev_personalize` `0x400987a2`.
  Two Digitone II events of its own: `ev_midi_step` (`0x400da0b2` in the
  step handler and `0x400d9230` in the pending-step pass, both where the
  sequencer builds a MIDI-machine track's event with `0x400d8cbe`) and
  `ev_midi_note` (`0x4012b490`, in the MIDI task's note loop, before its
  range check). No explicit branch and no switch case lands inside any of
  the ten sites. Checked by cold boots in digikit's emulator (6 Oct 2026):
  see mods/core-dn2/README.md.
- **Not checked yet:** the version a unit reports (on the Digitakt II, the
  container's); a build with mods on a unit; anything the
  emulator does not reach (DSP audio, USB, Overbridge, sample and project
  I/O on a filled +Drive).

## The Digitone mk1 and Digitone Keys (1.43)

- **Files.** One `.syx` serves both. It is the Digitakt mk1's family (ELE3,
  the same SysEx transport and layout) with seven sections: 5, 2 (the
  bootstrap, with the startup menu), 3 (MAIN OS), 4 (the updater), 6, 7 (the
  second CPU's image, which renders the FM voices) and 8. The longer table
  moves the first section to `0xA0`; the reader and writer take that from
  the stock file. From its own main OS stream, the writer reproduces the
  stock file byte for byte.
- **Unpacking.** The bootstrap loads the updater, which reads section 3
  from flash (offset + `0x80000`) to `0x40200000` and depacks it in place to
  `0x40000400`: the Digitakt mk1's loader, instruction for instruction
  (`0x80003518` in the updater). Stock's in-place gap is 403,540 bytes.
- **Recovery.** The startup menu (FUNC at power-on) has 4 ... OS UPGRADE.
- **Checked in an emulator.** digikit's firmware check runs the file's own
  bootstrap and updater, then a cold boot and a scripted session. Stock 1.43,
  repacked by elekloader, passes it against stock: the updater accepts the
  stream, and the screens and the audio are identical.
- **Linkable mods.** Its core is `mods/core-dn1`: `core.s` with the
  Digitone's addresses. Its eight sites and every routine it calls are the
  Digitakt mk1's found again in 1.43. They are the same code instruction
  for instruction, bar the addresses in it. The render handler's frame is
  0xB8 bytes (0xA8 on the Digitakt), hence `RENDER_FRAME`. From 2.1
  (`voice.s`) it has a ninth site, the render's voice note-on at
  0x4009e928, for the Digitone-only `ev_voice_on`, and parameter slots
  (`params.s`): ids 182-184 for mods, through 63 more sites; mod pages
  (`pages.s`): pages 27-30, through two more; and project data
  (`projdata.s`): mods' blocks saved with the project, through five more.
  From 2.2 the Mod Menu (`menu.s`): holding a track key opens a menu of
  the mods' entries; the four UI sites go to its wrappers first.
  1.44 has all of them, at its own addresses (`ports`).
- **The DDR area** is the Digitakt's, `0x47BE0000-0x47C00000`:
  - The OS clears `0x4028E000-0x43229E60` at start, and its stack runs down
    from `0x48000000`.
  - No instruction operand in its code points between `0x43EB0000` and the
    stack.
  - In a settled emulator run, no DDR page from `0x43400000` to
    `0x47C00000` is ever mapped.
- **Checked in the emulator:** with core alone, the build passes every
  stage, with every screen identical to stock.
  - The audio is identical to stock up to PLAY.
  - After PLAY it is the same sound, sample-shifted, because core's few
    cycles move when the scripted key presses land. Stock against itself
    with PLAY 0.3 ms later diverges in the same way.

## The Octatrack (1.40C)

- **Files.** One stock image serves the MKI and MKII. It comes as a `.syx`
  (the legacy SysEx transport) and as a `.bin` card file (ELUP: a
  word-feedback cipher with a checksum). Both carry one ELEK container,
  whose only section is the main OS, at `0x40000400`. `elek.py` rebuilds
  both stock files byte for byte from their own main OS stream.
- **Protected.** `0x400de1e0-0x400e21e0` holds the copy of the bootloader
  that the OS can re-flash. No site, blob or verified output may change it.
  - The OS's USB descriptor tables are its top 480 bytes, from
    `0x400e2000`. The device descriptor (18 bytes) is `relocatable`: the
    only pointer to it is the operand of `pea 0x400e2000` at `0x4001d82c`
    (GET_DESCRIPTOR), and nothing writes it. A mod may point that operand
    at its own copy. tests/test_octatrack.py checks, against the stock
    file, that no other four bytes outside the range point into it.
- **Flash.** The container sits at `0x4000` and must end below `0x200000`.
- **Not known yet.** Where the bootloader stages the image (`stage` is
  `None`), so the in-place unpack is not simulated.
- **Linkable mods.** There is no free RAM in stock: the OS gives the whole
  audio page arena (`0x40a955e0-0x46025de0`, 14,602 pages of 6,144 B) to
  samples and the recorders. The core, `mods/core-ot`, takes the arena's
  bottom 1,707 pages (10 MB), as sambanks/octabam's platform does, and
  that is the device's `ddr`: `0x40a955e0-0x41495de0`.
  - It moves the arena's base past them with octabam's 28 arena writes
    (the base, base + one page, the page count, the fill limit and the
    clear length). tests/test_octatrack.py derives each from the page count.
  - Its `.boot` is appended at the image's end (`0x4010fdf0`) and called
    from the boot site `0x4000050c`, in place of `0x40001e50`, which it
    calls first. It copies the run image to the reserve through the
    uncached alias (`+0x08000000`) and zeroes `.bss`.
  - Its hook bus (0.2) has six events of the Octatrack's own: the sys
    task's tick, each composed frame, the keys, the encoders, MIDI in and
    the frame interrupt (docs/ADAPTING.md, "The hook bus on the
    Octatrack"). Its sites keep clear of every byte the converted octabam
    modules patch, so those link with 0.2 as they did with 0.1.
  - The OS composes the screen twice before the boot site, so before the
    run image is in the reserve. The draw site goes through a gate placed
    (`fixed`) at the end of the zero run at `0x400c45b0`, which waits for
    `.boot` to have run.
  - `elekloader.sdk.octabam` converts octabam's ColdFire-only modules to
    such mods, and whole remixes, which is how USB AUDIO IN converts
    (docs/ADAPTING.md, 4b).
- **Free space inside the image** (`image_free`, octabam's measured zero
  runs): `0x400c45b0-0x400c4702`, `0x400d24d0-0x400d2ce0` and
  `0x400d64e0-0x400d7c3c`. octabam pins code in these runs, and `fixed`
  code may use them.
  - The third run starts at `0x400d64da` in stock. Its first six bytes are
    left out: they follow the table at `0x400d64a0`.
  - Left out entirely: `0x400d2ee6-0x400d3020` (a live descriptor), and
    everything above `0x400d8000` (the PROJECT subsystem's RAM).
- **Checked in an emulator** (octabam's `ot_emu`): with core and a mod that
  puts a marker in `.run`, the boot reaches the RTOS handoff as stock does,
  `.boot` runs once, and the marker is in the reserve after boot.
- **On a unit** (an MKII, 3 Oct 2026): eight builds flashed from the card
  booted, each showing its own version.
  - The builds were core alone, then core with 19 of the converted octabam
    mods (irpina/octabam2elemod v1.0) in seven groups, the last with all 19.
  - In the last build, the 15 mods it carried were tested in use, and all
    worked: the 14 non-USB mods but SCENES P2, and USB IO TRACKS MAIN CUE AB.
    The shop's details sheet says, mod by mod, what has been checked
    (`on_unit` in web/catalog.json).
  - Not yet run on an MKI.

## What a new device needs besides its profile

- **Sealed files.** `trailer='hmac'` with `hmac_key_from` (the Digitakt
  II, above). Another sealed device needs its seed found in its bootstrap
  (the Digitone II's is `"Multiplier"`, digikit), and the writer checked to
  reproduce its stock file, seal included.
- **The packing format.** The loader repacks with the same codec
  (`codec/`). Check that the device's own depacker accepts it: every
  section must round-trip through `codec/elz.py`, and the in-place
  simulation must hold for the device's `stage` and every one of its
  `more_stages`. Look for a second loader (the Digitone II's section 4 has
  one), and for a receive buffer in the OS's upgrade path that caps the
  file (`receive_max`); `packer='dense'` packs tighter than aplib where
  that cap needs it.
- **Another CPU.** A decoder in `isa/` with `reader()`, `decode()` and the
  flags `ILLEGAL`, `LINEF`, `FPU`, `PCREL` and the flow constants.
- **Tests.** Before anything is flashed:
  - the writer must reproduce a stock file from its own main OS stream
    (`test_writer_no_change_is_stock`);
  - a verified build must pass the device's cold boot in an emulator, if
    one exists.

Mods are made per release: a mod's target names one stock file by hash, so
supporting a new OS version means adding its release to the profile and
rebuilding the mods for it (below).

## A new OS version of a supported device

How Digitakt mk1 1.54 and Digitone mk1 1.44 were added (2026-10-01):

1. **Compare the file with the last release's.** Every section but the main
   OS (and section 5, the version string) must be the same bytes: the same
   bootstrap and updater, so the same staging and flash layout. For both
   releases that held. If the bootstrap changes, check `stage`,
   `flash_limit` and the recovery path again.
2. **Add the release** to the profile: the file's sha256, and the main OS's
   sha256 and length, depacked. The writer must reproduce the file from its
   own main OS stream, and its main OS must depack in place from `stage`
   (`tests/test_releases.py` checks both for every release given).
3. **Check the memory facts** the profile states: here, the same SRAM
   operands in the code, and no operand near the DDR area mods run from.
   Both releases moved their RAM past the image by 0x1000 and changed
   nothing else there.
4. **Port the cores**, under `ports` in their `mod.json` (ADAPTING.md 3.10):
   every `defsym` and site found again in the new image, and each site's
   stock bytes read from it. `tests/test_releases.py` builds and lints every
   core for every release it names.
5. **Run a build with the core in the emulator** against the new stock
   file. Digitakt mk1 1.54: core 2.1 passes, with every screen identical
   and the audio identical apart from the recording's silent end, 1 ms
   off, exactly as core 2.1 does on 1.53. Digitone mk1 1.44: not yet,
   since the emulator does not boot stock 1.44 to a settled screen.
6. **Tell mod authors** to build their mods for the new OS. A mod for the
   old one is refused with the new stock file.
