# SPDX-License-Identifier: GPL-2.0-or-later
"""The Digitone II: its profile, its sealed file, its two loaders, the OS's
receive buffer, whole builds (pytest, or run with python).

Needs the stock file, which never goes in the repo, named by an environment
variable; a test whose input is missing is skipped, not passed:
  ELEKLOADER_DN2_SYX   Digitone_II_OS1_12.syx
"""
import hashlib
import hmac
import json
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from elekloader import devices, elemod, formats, link, patch, syx   # noqa: E402
from elekloader.codec import aplib, dense, transport                # noqa: E402
from elekloader.elemod import sha                                   # noqa: E402

SYX = os.environ.get('ELEKLOADER_DN2_SYX', '')
DEV = [d for d in devices.devices() if d.key == 'digitone-mk2'][0]
REL = DEV.releases['1.12']
SECTION4 = 0x40200000                  # section 4's loader stages here (DEV.more_stages)


class Skip(Exception):
    pass


def need(p):
    if not p or not os.path.exists(p):
        raise Skip('missing %s' % (p or 'ELEKLOADER_DN2_SYX'))
    with open(p, 'rb') as fh:
        return fh.read()


_c = {}


def stock():
    if 'st' not in _c:
        _c['raw'] = need(SYX)
        _c['st'], dev, rel = formats.load(_c['raw'])
        assert dev is DEV and rel == REL
        _c['img'] = formats.main_image(_c['st'], DEV)
    return _c['st'], _c['img']


def expect(fn, *words):
    try:
        fn()
    except (formats.FormatError, elemod.ModError, patch.PatchError, syx.SyxError) as e:
        for w in words:
            assert w in str(e), 'expected %r in: %s' % (w, e)
        return str(e)
    raise AssertionError('not refused')


def bundle(sites=(), blob=b'', version=None):
    """A format-1 mod for the Digitone II: data `sites` [(addr, new)], a blob."""
    st, img = stock()
    doc = {'elemod': 1, 'id': 'dn2-test', 'version': '1', 'target': devices.target_of(DEV, REL),
           'sites': []}
    for addr, new in sites:
        o = addr - DEV.main_load
        doc['sites'].append({'addr': '0x%08x' % addr, 'len': len(new), 'kind': 'data',
                             'stock_sha256': sha(img[o:o + len(new)]), 'new': new.hex()})
    if blob:
        doc['blob'] = {'load': '0x%08x' % DEV.image_end(REL), 'len': len(blob),
                       'sha256': sha(blob), 'parts': [['hex', blob.hex()]]}
    if version:
        doc['ele3_version'] = version
    return doc


def a_string_site(text=b'FACTORY RESET'):
    """(address, same-length replacement) for a string of the main OS."""
    st, img = stock()
    o = img.find(text)
    assert o > 0, text
    return DEV.main_load + o, text.lower()


def write_mod(tmp, doc, name='dn2-test.elemod'):
    p = os.path.join(tmp, name)
    with open(p, 'w') as fh:
        json.dump(doc, fh)
    return p


def build(doc, version=None):
    with tempfile.TemporaryDirectory() as tmp:
        return patch.build(SYX, [write_mod(tmp, doc)], version=version, log=lambda *a: None)


def reseal(st, container):
    """-> .syx bytes for `container` (an ELE3 container, trailer included)."""
    stream = (len(container).to_bytes(4, 'big')
              + transport.content_checksum(container).to_bytes(4, 'big') + container)
    return transport.encode_syx(stream, st.device_id, st.framing[0], st.framing[1])


def word(img, addr, n=4):
    return int.from_bytes(img[addr - DEV.main_load:addr - DEV.main_load + n], 'big')


# ---- the file ------------------------------------------------------------------------------

def test_stock_file_parses():
    st, img = stock()
    assert st.device_id == 0x15 and st.version == '1.12' and st.data_start == 0x90
    assert [t[0] for t in st.table] == [5, 2, 3, 4, 7, 8]
    assert st.total == 0x1CC740 and len(st.stored[3]) == 1131348
    assert len(img) == REL.main_len and sha(img) == REL.main_sha256
    assert DEV.image_end(REL) == 0x4030C980


def test_the_seal_is_the_last_32_bytes_and_verifies():
    st, img = stock()
    end = max(off + n for _s, off, n, _d in st.table)
    assert st.total == end + (-end % 16) + syx.DIGEST
    key = syx.seal_key(st, DEV)
    assert hmac.new(key, st.container[:-32], hashlib.sha256).digest() == st.container[-32:]


def test_writer_no_change_is_stock():
    """From its own main OS stream, the writer gives the stock file byte for byte."""
    st, img = stock()
    assert formats.write(st, st.stored[3], DEV)['syx'] == _c['raw']


def test_the_stock_file_verifies_at_both_stages():
    """Elektron's stream: in place from 0x40400000 (the bootstrap) and from
    0x40200000 (section 4), and 18,674 chunks in the 2 MiB receive buffer."""
    st, img = stock()
    assert DEV.stages() == (0x40400000, SECTION4)
    for stage, want in ((0x40400000, 2128334), (SECTION4, 31182)):
        out, gap = syx.inplace_depack(st.stored[3], DEV, stage)
        assert out == img and gap == want, (hex(stage), gap)
    f = formats.verify({'syx': _c['raw']}, st, img, DEV)
    assert f['main']['inplace_gaps'] == {'0x40400000': 2128334, '0x40200000': 31182}
    assert f['main']['inplace_min_gap'] == 31182
    assert f['received'] == 18674 * 101 and f['receive_headroom'] == 0x200000 - 18674 * 101


def test_verify_refuses_a_broken_seal():
    st, img = stock()
    bad = bytearray(st.container)
    bad[-1] ^= 1
    expect(lambda: syx.verify(reseal(st, bytes(bad)), st, img, DEV), 'HMAC')
    syx.verify(reseal(st, st.container), st, img, DEV)        # the unbroken one passes


# ---- elekloader's own builds -----------------------------------------------------------------

def test_a_no_change_build_verifies():
    """The stock image, packed as elekloader packs it for this device
    (codec/dense.py): it verifies, depacks to the stock image, keeps a gap
    at both stages and fits the receive buffer."""
    st, img = stock()
    stored = formats.pack_main(img, DEV)
    out = formats.write(st, stored, DEV, 'DN01')
    f = formats.verify(out, st, img, DEV, 'DN01')
    assert f['main']['sha256'] == REL.main_sha256 and f['ele3_version'] == 'DN01'
    gaps = f['main']['inplace_gaps']
    assert gaps['0x40200000'] > 0 and gaps['0x40400000'] == gaps['0x40200000'] + 0x200000
    assert f['container_len'] <= 0x200000 and f['receive_headroom'] > 0
    o = formats.parse(out['syx'], DEV)
    assert all(o.stored[k] == st.stored[k] for k in (5, 2, 4, 7, 8))   # the rest stays stock
    assert o.table[1][3] == 0x02010000                              # the bootstrap's version


def test_the_aplib_stream_is_refused_by_the_receive_buffer():
    """Why the device's packer is 'dense': codec/aplib.py's stream of the
    stock image puts the container over the 2 MiB the OS receives into.
    verify refuses it there, though both stages would pass."""
    st, img = stock()
    stored = aplib.pack_section(img)
    for stage in DEV.stages():
        assert syx.inplace_depack(stored, DEV, stage)[1] > 0
    out = syx.write(st, stored, DEV)
    assert len(syx.Syx(out).data) * 101 > 0x200000
    expect(lambda: syx.verify(out, st, img, DEV), 'receive buffer', '0x200000')


def test_a_build_past_the_receive_buffer_is_refused():
    """A whole build whose incompressible blob takes the container past the
    buffer, through patch.build."""
    st, img = stock()
    room = 0x200000 - len(st.data) * 101
    expect(lambda: build(bundle([], os.urandom(room + 0x1000), version='DN02')),
           'failed verification', 'receive buffer')


def test_a_negative_inplace_gap_at_section_4s_stage_is_refused():
    """120 KB of zeros appended: a stream packed without the floor keeps its
    gap at 0x40400000 but not at 0x40200000, and verify refuses it there;
    pack_main's floor keeps both, with literals."""
    st, img = stock()
    big = img + bytes(120 * 1024)
    plain = dense.pack_section(big)[0]
    assert syx.inplace_depack(plain, DEV)[1] > 0                     # the bootstrap's stage
    assert syx.inplace_depack(plain, DEV, SECTION4)[1] <= 0
    expect(lambda: syx.verify(syx.write(st, plain, DEV), st, big, DEV),
           'from 0x40200000', 'overwrites unread input')
    floored = formats.pack_main(big, DEV)
    assert len(floored) > len(plain)
    f = formats.verify(formats.write(st, floored, DEV), st, big, DEV)
    assert f['main']['inplace_gaps']['0x40200000'] > 0 and f['receive_headroom'] > 0


def test_the_two_limits_cap_how_far_the_image_can_grow():
    """Section 4's stage needs the stream to stay ahead of the image's end
    (stream > image - 0x1ffc00), and the buffer caps the stream: so even
    zeros, which pack to almost nothing, can add only about 240 KB. 300 KB
    of them, floored, overrun the buffer and are refused."""
    st, img = stock()
    big = img + bytes(300 * 1024)
    floored = formats.pack_main(big, DEV)
    out, gap = syx.inplace_depack(floored, DEV, SECTION4)
    assert out == big and gap > 0                                  # the floor holds ...
    expect(lambda: formats.verify(formats.write(st, floored, DEV), st, big, DEV),
           'receive buffer')                                       # ... the buffer does not


def test_whole_build_data_site_and_blob():
    st, img = stock()
    site = a_string_site()
    blob = bytes(range(256)) * 8
    outputs, man = build(bundle([site], blob, version='DN03'))
    assert set(outputs) == {'syx'}
    o = formats.parse(outputs['syx'], DEV)
    new = formats.main_image(o, DEV)
    assert new[site[0] - DEV.main_load:][:len(site[1])] == site[1]
    assert new[len(img):] == blob and len(new) == len(img) + len(blob)
    assert o.version == 'DN03'
    assert all(o.stored[k] == st.stored[k] for k in (5, 2, 4, 7, 8))
    assert hmac.new(syx.seal_key(st, DEV), o.container[:-32],
                    hashlib.sha256).digest() == o.container[-32:]
    gaps = man['output']['main']['inplace_gaps']
    assert set(gaps) == {'0x40400000', '0x40200000'} and min(gaps.values()) > 0


def test_linkable_mods_need_the_digitone_ii_core():
    """The Digitone II links format-2 mods in the top megabyte of its free DDR,
    with its own core (mods/core-dn2), which declares and copies core_fast."""
    assert DEV.linkable() and DEV.ddr == (0x47E00000, 0x47F00000)
    assert DEV.sram_code == DEV.areas['sram-tail'] and DEV.fast_table == 'core_fast'
    lo, hi = DEV.areas['ddr']
    assert hi <= DEV.ddr[0] or DEV.ddr[1] <= lo          # no region can overlap the linker's
    addr, new = a_string_site()
    st, img = stock()
    o = addr - DEV.main_load
    doc = {'elemod': 2, 'id': 'x', 'version': '1', 'target': devices.target_of(DEV, REL),
           'sections': {}, 'requires': ['core'],
           'sites': [{'addr': '0x%08x' % addr, 'len': len(new), 'kind': 'data',
                      'stock_sha256': sha(img[o:o + len(new)]), 'new': new.hex()}]}
    problems = link.check([link.Mod2(doc, 'x')], img)
    assert any('requires core' in p for p in problems), problems


def built(moddir, tmp):
    """-> the .elemod mod.json in `moddir` builds to (needs the cross toolchain)."""
    import shutil
    from elekloader.sdk import build
    need(SYX)
    if not shutil.which(os.environ.get('ELEKLOADER_CROSS', DEV.toolchain['prefix']) + 'as'):
        raise Skip('no cross assembler (ELEKLOADER_CROSS)')
    return build.build(os.path.join(os.path.dirname(HERE), moddir), SYX, tmp)[0]


# core-dn2's sites and their stock bytes: the Digitakt II core's six and its boot site, found
# again, and the three MIDI sites (two for ev_midi_step, one for ev_midi_note)
CORE_SITES = ((0x40000538, '4eb9400019b0'), (0x4002e464, '4eb94011d5d4'),
              (0x4002e4ca, '4eb94011d60a'), (0x4002f20c, '4eb94002c5c8'),
              (0x4002f256, '4eb94002c55c'), (0x4009ff88, '205248780002'),
              (0x400987a2, '4cef7c7c0018'), (0x400da0b2, '2f024ebaec08'),
              (0x400d9230, '2f024ebafa8a'), (0x4012b490, '727fc2802841'))


def test_core_builds_links_and_verifies():
    """mods/core-dn2 built with the SDK, then a build with it alone: its ten
    sites, and nothing else in the image changed; both stages keep a gap."""
    with tempfile.TemporaryDirectory() as tmp:
        core = built(os.path.join('mods', 'core-dn2'), tmp)
        outputs, man = patch.build(SYX, [core], version='DC03', log=lambda *a: None)
    st, img = stock()
    new = formats.main_image(formats.parse(outputs['syx'], DEV), DEV)
    changed = [i for i in range(len(img)) if new[i] != img[i]]
    for site, stock_bytes in CORE_SITES:
        a = site - DEV.main_load
        assert img[a:a + 6].hex() == stock_bytes, hex(site)
        assert new[a:a + 2] in (b'\x4e\xb9', b'\x4e\xf9') and new[a:a + 6] != img[a:a + 6], hex(site)
    sites = [s - DEV.main_load for s, _ in CORE_SITES]
    assert all(any(s <= i < s + 6 for s in sites) for i in changed), [hex(DEV.main_load + i)
                                                                      for i in changed[:8]]
    assert man['link']['layout']['ddr'][0] == DEV.ddr[0]
    gaps = man['output']['main']['inplace_gaps']
    assert set(gaps) == {'0x40400000', '0x40200000'} and min(gaps.values()) > 0


def test_the_examples_link_with_core():
    """examples/hello-marker-dn2 (ev_draw) and examples/midi-hooktest-dn2
    (ev_midi_step, ev_midi_note), each with core: no sites of their own."""
    with tempfile.TemporaryDirectory() as tmp:
        core = built(os.path.join('mods', 'core-dn2'), tmp)
        hello = built(os.path.join('examples', 'hello-marker-dn2'), tmp)
        midi = built(os.path.join('examples', 'midi-hooktest-dn2'), tmp)
        outputs, man = patch.build(SYX, [core, hello, midi], version='DC04', log=lambda *a: None)
    tables = man['link']['tables']
    assert tables['ev_draw']['entries'] == 1
    assert tables['ev_midi_step']['entries'] == 1 and tables['ev_midi_note']['entries'] == 1
    st, img = stock()
    new = formats.main_image(formats.parse(outputs['syx'], DEV), DEV)
    changed = {i for i in range(len(img)) if new[i] != img[i]}
    sites = {s - DEV.main_load + k for s, _ in CORE_SITES for k in range(6)}
    assert changed <= sites


def test_fx_rename_only_repoints_names():
    """examples/fx-rename-dn2 with core: outside core's sites, only 4-byte pointers and `pea`
    operands change, and each new pointer leads to a string in the mod's RAM image that the
    linker placed at 0x47E00000 (the descriptor table's CC words and every other field stay)."""
    with tempfile.TemporaryDirectory() as tmp:
        core = built(os.path.join('mods', 'core-dn2'), tmp)
        ren = built(os.path.join('examples', 'fx-rename-dn2'), tmp)
        outputs, man = patch.build(SYX, [core, ren], version='FTN0', log=lambda *a: None)
    st, img = stock()
    new = formats.main_image(formats.parse(outputs['syx'], DEV), DEV)
    with open(os.path.join(os.path.dirname(HERE), 'examples', 'fx-rename-dn2', 'mod.json')) as fh:
        doc = json.load(fh)
    core_bytes = {s - DEV.main_load + k for s, _ in CORE_SITES for k in range(6)}
    allowed = set(core_bytes)
    lay = man['link']['layout']
    run_lo, run_hi = lay['ddr']
    run_img = new[lay['run_load'] - DEV.main_load:]
    for s in doc['sites']:
        a = int(s['addr'], 16) - DEV.main_load
        assert s['op'] in ('ptr', 'keep2'), s
        if s['op'] == 'keep2':
            assert img[a:a + 2] == new[a:a + 2] == b'\x48\x79', s['addr']        # pea (abs).l
            a += 2
        allowed |= set(range(a, a + 4))
        p = word(new, DEV.main_load + a)
        assert run_lo <= p < run_hi, s['addr']
        text = run_img[p - run_lo:p - run_lo + 24].split(b'\0')[0]
        assert text and all(32 <= c < 127 for c in text), (s['addr'], text)
    end = len(img)
    changed = {i for i in range(end) if new[i] != img[i]}
    assert changed <= allowed, [hex(DEV.main_load + i) for i in sorted(changed - allowed)[:8]]


# ---- protected ranges -------------------------------------------------------------------------

def test_the_panel_firmware_range_is_what_the_updater_sends():
    """FUN_400dbbd8 sends 0x402fb494 - 0x402f7d40 bytes (move.l #0x402fb494,d3
    ; subi.l #0x402f7d40,d3): the words of run address 0xC004 up, read from
    the image at run address + 0x402ebd3c + 4, then the header word at 0xC000
    (0x402f7d40). Its version check reads 0x402f7d44 (0x15)."""
    st, img = stock()
    lo, hi, why = DEV.protected[1]
    assert 'panel' in why and (lo, hi) == (0x402F7D40, 0x402FB494)
    assert word(img, 0x402F7D40) == 0xE778 and word(img, 0x402F7D44) == 0x15
    assert img[0x400DBCC8 - DEV.main_load:][:6] == bytes.fromhex('263c402fb494')
    assert img[0x400DBCD0 - DEV.main_load:][:6] == bytes.fromhex('0483402f7d40')
    assert img[0x400DBD1E - DEV.main_load:][:6] == bytes.fromhex('d1fc402ebd3c')
    assert img[0x400DBC30 - DEV.main_load:][:6] == bytes.fromhex('2439402f7d44')
    assert img[0x400DBD66 - DEV.main_load:][:6] == bytes.fromhex('2f39402f7d40')
    lo, hi, why = DEV.protected[0]
    assert lo <= 0x400DBBD8 and 0x400DBDAE <= hi and 'updater' in why


def test_a_site_in_a_protected_range_is_refused():
    st, img = stock()
    for addr in (0x402F7D44, 0x400DBBD8, 0x401354D6, 0x40139104):
        expect(lambda: build(bundle([(addr, b'\x00\x00\x00\x00')], version='DN04')),
               'protected on the Digitone II')
    changed = bytearray(img)
    changed[0x402F7D47 - DEV.main_load] = 0x16                     # the panel version word
    out = formats.write(st, formats.pack_main(bytes(changed), DEV), DEV)
    expect(lambda: formats.verify(out, st, bytes(changed), DEV), 'is not stock', 'panel')


def test_every_flash_write_and_erase_in_the_os_is_known():
    """The SPI NOR driver's program, erase and protection entries are named
    (jsr, bsr, lea, pea or an immediate) only inside the driver, by the
    settings store (0x400cccc6-0x400cd21e), the factory console
    (0x400cd5f2-0x400cebd2), the COKi migration (0x400f2022-0x400f2168) and
    the in-OS upgrade writer (0x40139104-0x4013924c), which runs the
    validator 0x400dbdb0 first and writes from 0x80000. The driver and the
    writer are protected."""
    st, img = stock()
    entries = (0x401354D6, 0x40135618, 0x40134A6A, 0x401357CE, 0x40135850, 0x401360A6,
               0x40136122, 0x4013618C, 0x40135F80, 0x40135CA6, 0x40135DC8, 0x4013517E,
               0x401352B8, 0x40135E7C, 0x40135BA2)
    known = [(0x40134A28, 0x4013621C), (0x400CCCC6, 0x400CD21E), (0x400CD5F2, 0x400CEBD2),
             (0x400F2022, 0x400F2168), (0x40139104, 0x4013924C)]
    code_end = 0x401D0800 - DEV.main_load
    seen = 0
    for i in range(0, code_end - 4, 2):
        a = DEV.main_load + i
        if int.from_bytes(img[i:i + 4], 'big') in entries:
            site = a - 2
        elif img[i:i + 2] in (b'\x4e\xba', b'\x61\x00', b'\x60\x00') and \
                a + 2 + int.from_bytes(img[i + 2:i + 4], 'big', signed=True) in entries:
            site = a
        else:
            continue
        seen += 1
        assert any(lo <= site < hi for lo, hi in known), hex(site)
    assert seen == 49
    assert word(img, 0x40139114 + 2) == 0x400DBDB0                       # the validator first
    assert img[0x4013912E - DEV.main_load:][:6] == bytes.fromhex('267c00080000')   # a3 = 0x80000
    driver = [p for p in DEV.protected if p[0] == 0x40134A28 and p[1] == 0x4013621C]
    writer = [p for p in DEV.protected if p[0] == 0x40139104 and p[1] == 0x4013924C]
    assert driver and writer


# ---- the loaders, the flash, the receive buffer ------------------------------------------------

def test_the_bootstrap_stages_at_stage_from_flash_at():
    """The loader 0x80000596 (section 2 runs at 0x80000400, after a 4-byte
    prefix): pea stage ; ... addi.l #flash_at, d0 ; jsr flash read ; ...
    pea stage ; bsr depack (0x80000432)."""
    st, img = stock()
    boot = st.section(2)
    at = lambda a: a - 0x80000400 + 4
    assert boot[at(0x800005c6):at(0x800005c6) + 6] == bytes.fromhex('4879') + DEV.stage.to_bytes(4, 'big')
    assert boot[at(0x800005d4):at(0x800005d4) + 6] == bytes.fromhex('0680') + DEV.flash_at.to_bytes(4, 'big')
    assert boot[at(0x800005e6):at(0x800005e6) + 6] == bytes.fromhex('4879') + DEV.stage.to_bytes(4, 'big')
    bsr = at(0x800005ec)
    assert boot[bsr:bsr + 2] == b'\x4e\xba'
    assert 0x800005ee + int.from_bytes(boot[bsr + 2:bsr + 4], 'big', signed=True) == 0x80000432


def test_section_4_stages_at_the_second_stage_and_depacks_in_place():
    """Section 4 runs at 0x80000400, its stored bytes header included. Its
    loader 0x8000357c: pea 0x40200000 ; ... addi.l #0x80000, d1 ; jsr flash
    read ; ... pea 0x40200000 ; jsr 0x80005728, a byte-identical copy of the
    bootstrap's depacker (as is the main OS's, 0x40136820). The bootstrap
    runs section 4 (jsr 0x8000f100) only when boot flag bit 6 is set."""
    st, img = stock()
    s4 = st.stored[4]
    at = lambda a: a - 0x80000400
    pea = bytes.fromhex('4879') + SECTION4.to_bytes(4, 'big')
    assert (SECTION4, DEV.more_stages[0][1]) == DEV.more_stages[0] and '0x8000357c' in DEV.more_stages[0][1]
    assert s4[at(0x80003590):at(0x80003590) + 6] == pea
    assert s4[at(0x8000359e):at(0x8000359e) + 6] == bytes.fromhex('0681') + DEV.flash_at.to_bytes(4, 'big')
    assert s4[at(0x800035b0):at(0x800035b0) + 6] == pea
    assert s4[at(0x800035b6):at(0x800035b6) + 6] == bytes.fromhex('4eb980005728')
    boot = st.section(2)
    b = lambda a: a - 0x80000400 + 4
    depacker = boot[b(0x80000432):b(0x80000432) + 356]
    assert depacker[-2:] == b'\x4e\x75'
    assert s4[at(0x80005728):at(0x80005728) + 356] == depacker
    assert img[0x40136820 - DEV.main_load:][:356] == depacker
    # moveq #64,d0 ; and.l 0x80008404,d0 ; beq.s +6 ; jsr 0x8000f100
    assert boot[b(0x80000ee4):b(0x80000ee4) + 16] == bytes.fromhex('7040c0b98000840467064eb98000f100')


def test_the_settings_store_is_the_flash_limit():
    """The OS's records at flash 0x380000 and 0x3c0000 (move.l #imm,-(sp)
    before each call): the container must end below them."""
    st, img = stock()
    push = lambda v: bytes.fromhex('2f3c') + v.to_bytes(4, 'big')
    assert img.count(push(0x380000)) == 26 and img.count(push(0x3c0000)) == 12
    assert DEV.flash_limit == 0x380000 and (DEV.flash_limit - DEV.flash_at) % 0x40000 == 0
    assert DEV.receive_max < DEV.flash_limit - DEV.flash_at        # the buffer is the tighter


def test_the_receive_buffer_is_receive_max():
    """FUN_40122574 returns the receive state 0x4038bec0 (move.l #imm,d0 ;
    rts); FUN_4012257c writes message n's 101 bytes to state + 0x20 + n*0x65
    and never bounds n. The next object the code names, a SysEx transmit
    buffer at 0x4058bee0 (0x40000574), ends the 2 MiB."""
    st, img = stock()
    assert img[0x40122574 - DEV.main_load:][:8] == bytes.fromhex('203c4038bec04e75')
    assert img[0x4000057e - DEV.main_load:][:6] == bytes.fromhex('41f94058bee0')
    assert DEV.receive_max == 0x4058BEE0 - (0x4038BEC0 + 0x20)


def test_the_areas_are_outside_what_the_os_clears_and_copies():
    """The reset code (0x400004e8): it copies 0x402fd000-0x40305000 to SRAM
    0x80000000 and 0x40305000-0x4030c980 to 0x80008000, zeroes the rest to
    0x80010000, clears DDR 0x402fd000-0x466b84d0, and sets the stack to
    0x48000000. The DDR area lies above the clear and the DMA buffers the
    code names through the uncached alias (up to 0x4e709340), below the
    boot stack's 1 MB; the SRAM area is the zeroed tail."""
    st, img = stock()
    assert word(img, 0x4000045e + 2) == 0x402FD000 and word(img, 0x40000464 + 2) == 0x80000000
    assert word(img, 0x40000470 + 2) == 0x40305000 and word(img, 0x4000047e + 2) == 0x80008000
    assert word(img, 0x40000486 + 2) == 0x40305000 and word(img, 0x4000048c + 2) == 0x80008000
    assert word(img, 0x40000498 + 2) == 0x4030C980 and word(img, 0x400004a6 + 2) == 0x80010000
    assert word(img, 0x400004ba + 2) == 0x402FD000 and word(img, 0x400004c0 + 2) == 0x466B84D0
    assert word(img, 0x400004f2 + 2) == 0x48000000
    assert word(img, 0x4000399c + 2) == 0x4E709300                 # 16 longs: the highest buffer
    assert not any(img[0x402FD000 - DEV.main_load:])               # the SRAM copies are zeros
    lo, hi = DEV.areas['ddr']
    assert 0x466B84D0 <= lo and 0x4E709340 - 0x08000000 <= lo and hi <= 0x48000000 - 0x100000
    tail = 0x80008000 + (0x4030C980 - 0x40305000)
    assert DEV.areas['sram-tail'] == (tail, 0x80010000)
    # the highest SRAM array: rows of 202 halfwords, each stored as a long (404 bytes), from
    # 0x8000dea4 (FUN_400daffc: move.l #202,d1 ; ... addi.l #0x8000dea4,d0); row 16 is the
    # last its callers write (FUN_40025444 passes 0x10)
    assert word(img, 0x400daffe + 2) == 202 and word(img, 0x400db014 + 2) == 0x8000DEA4
    assert 0x8000DEA4 + 17 * 404 <= tail


if __name__ == '__main__':
    import traceback
    ok = skipped = failed = 0
    for n in sorted(k for k in globals() if k.startswith('test_')):
        try:
            globals()[n]()
            print('ok      %s' % n)
            ok += 1
        except Skip as e:
            print('SKIP    %s (%s)' % (n, e))
            skipped += 1
        except Exception:
            print('FAIL    %s' % n)
            traceback.print_exc()
            failed += 1
    print('%d passed, %d failed, %d skipped' % (ok, failed, skipped))
    sys.exit(1 if failed else 0)
