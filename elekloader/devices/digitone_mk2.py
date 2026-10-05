# SPDX-License-Identifier: GPL-2.0-or-later
"""Digitone II: an NXP MCF5441x (ColdFire V4m, no FPU), like the Digitakt II;
the main OS runs from DDR at 0x40000400. Measured on OS 1.12, with the
Digitakt II 1.17 profile (digitakt_mk2.py) as the map and digikit's emulator
(https://github.com/m-dwyer/digikit) to run it. docs/DEVICES.md has the
evidence; tests/test_digitone2.py checks each fact against the stock file.

- The container: sections 5, 2 (the bootstrap; its `dest` is a version,
  0x0201, and it runs at 0x80000400), 3 (MAIN OS), 4 (a factory board test
  with a second main OS loader, below), 7 (the SHARC DSP's program) and 8
  (firmware the OS sends to a USB accessory). Section 3 is packed in the
  aPLib-shaped codec, 3,196,288 bytes depacked: it ends at 0x4030C980.
- Sealed as the Digitakt II's are: the seed in section 2 is "Multiplier".
  From a stock file's own main OS stream, syx.write reproduces that file
  byte for byte, seal included.
- Two loaders unpack the main OS, so both stages are checked:
  - the bootstrap's (0x80000596) reads section 3 from flash, at its offset
    + 0x80000, to 0x40400000, and depacks it to 0x40000400 (0x80000432);
    stock's in-place gap there is 2,128,334 bytes;
  - section 4's (0x8000357c) reads it to 0x40200000 and depacks it in place
    (0x80005728). The bootstrap runs section 4 instead when its power-on key
    read from the panel times out (boot flag bit 6, set only at 0x80000b9a).
    Stock's gap there is 31,182 bytes, and elektron-firmware-tool's default
    level gives -39,318: that file depacks wrong on that path and hangs. So
    `more_stages` names it, and verify needs a gap > 0 at both.
- The packer: packed by codec/aplib.py, the stock image's section is
  1,392,294 bytes, 260,946 more than Elektron's 1,131,348, which puts the
  container at 2,146,960 bytes, over the OS's receive buffer (below).
  codec/dense.py packs it to 1,155,260 (gap at 0x40200000: 55,094), so
  packer is 'dense', with its in-place floor at 0x40200000. The two limits
  cap how far a build's image can grow: about 164 KB that does not pack,
  about 236 KB of zeros.
- Flash: the container starts at 0x80000. The OS's settings records are at
  0x380000 and 0x3c0000 (26 and 12 `move.l #imm,-(sp)` of each), so
  flash_limit is 0x380000.
- The receive buffer: an upgrade over USB or MIDI (Transfer's route) is
  received into the static buffer 0x4038bec0 (FUN_40122574), the decoded
  stream from 0x4038bee0, one 101-byte chunk per message (FUN_4012257c, with
  no bound on the message count). The next object the code names is at
  0x4058bee0 (a SysEx transmit buffer, 0x40000574). So receive_max is
  0x200000: a bigger file overruns RAM during the upgrade. Stock fills
  1,886,074 bytes of it.
- At reset (0x400004e8), the OS copies 0x402fd000-0x40305000 to SRAM at
  0x80000000 and 0x40305000-0x4030c980 to 0x80008000 (both all zeros in
  the image), zeroes 0x8000f980-0x80010000, clears DDR 0x402fd000-
  0x466b84d0, and runs its stack down from 0x48000000.
- Free at run time (areas; no core yet, so ddr and sram_code are not set
  and format-2 mods are refused):
  - DDR 0x46800000-0x47F00000: above the clear's end and the DMA buffers
    (named only through the uncached alias, 0x4e6c0000-0x4e709340), below
    the boot stack's 1 MB. Above those buffers, no decoded instruction
    names 0x46709340-0x48000000 or its alias other than as a non-address
    constant (or the stack pointer's 0x48000000); no 1 MB
    page from 0x46700000 to 0x47efffff is ever touched (digikit maps a page
    on its first access) in a cold boot to 400M instructions and a session
    with key presses to 799M; and write watches over 0x46710000-0x47ff0000
    and its alias saw no write in either.
  - SRAM 0x8000f980-0x80010000: past the data the reset copies (whose
    highest array, 17 rows of 404 bytes from 0x8000dea4, ends at
    0x8000f978), zeroed by the reset, named by no decoded instruction, and
    written by nothing else in the same runs.
- Protected: the panel's firmware and the code that sends it (it runs on
  every normal boot, and the STARTUP menu needs the panel), the SPI NOR
  driver, and the in-OS upgrade writer, which has no length clamp.
"""
from . import Device, Release

DEVICE = Device(
    key='digitone-mk2',
    name='Digitone II',
    sysex_id=0x15,
    releases={
        '1.12': Release(
            version='1.12',
            syx_sha256='7e5b8a733d4ab1fdf7e5d22757f7d8eddaa3cbf71d0858874a791844c02ceb35',
            main_sha256='a098f620019b2ad38fd7e0397df39501fb1cf6bc26d0283915f34c49446fdf1a',
            main_len=3196288)
    },
    main_section=3,
    main_load=0x40000400,
    stage=0x40400000,                               # the bootstrap's loader, 0x80000596
    more_stages=(
        (0x40200000, "section 4's loader (0x8000357c), when the panel key read times out"),
    ),
    flash_at=0x80000,
    flash_limit=0x380000,
    receive_max=0x200000,                           # 0x4038bee0-0x4058bee0
    packer='dense',
    trailer='hmac',
    hmac_key_from=(2, b'Multiplier'),
    isa='coldfire',
    protected=(
        (0x400DBA28, 0x400DBDAE, 'the panel MCU updater FUN_400dbbd8 and its UART8 helpers, '
                                 'run on every normal boot'),
        (0x402F7D40, 0x402FB494, 'the panel MCU firmware the updater sends, with its header '
                                 '0x402f7d40 and version word 0x402f7d44'),
        (0x40134A28, 0x4013621C, 'the SPI NOR driver: every flash command and its wrappers'),
        (0x40139104, 0x4013924C, 'the in-OS upgrade writer FUN_40139104 (erases and writes '
                                 'from 0x80000, no length clamp)'),
    ),
    areas={
        'ddr': (0x46800000, 0x47F00000),          # above the DMA buffers, below the boot stack
        'sram-tail': (0x8000F980, 0x80010000),    # the SRAM's zeroed tail
    },
    recovery=('hold FUNC while powering on for the STARTUP menu, press TRIG 4 (OS UPGRADE), '
              'then send the stock .syx over DIN MIDI with Elektron Transfer (not USB: the '
              'bootstrap has no USB)'),
    toolchain={
        'prefix': 'm68k-linux-gnu-',                # binutils + gcc for m68k/ColdFire
        'asflags': ['-mcpu=54455'],                 # ColdFire V4 (the MCF5441x's ISA)
        'cflags': ['-mcpu=54455', '-O2', '-ffreestanding', '-fno-builtin', '-nostdlib',
                   '-fno-pic', '-fno-pie', '-fomit-frame-pointer', '-Wall'],
    },
)
