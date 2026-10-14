# SPDX-License-Identifier: GPL-2.0-or-later
"""The .elemod file: shared validation, format 1 (whole-build bundles), the
instruction-boundary check, and apply for bundles. Format 2 (separate,
linkable mods) is in link.py. The format itself is docs/FORMAT.md.

A .elemod is JSON. It carries only its author's bytes plus hashes of the
stock bytes it expects. It can only describe changes to the device's main
OS: there is no field for any other section, so the bootloader and the
other sections stay stock by construction (formats.verify checks the output
again).

Every mod names its "target": a stock release, by hash. The device profile
for that release (devices/) supplies the addresses, the free areas and the
instruction decoder.
"""
import hashlib
import json
import os

from . import devices

FORMAT = 1
KEY, LEGACY_KEY = 'elemod', 'dtmod'        # the format marker; files from before 0.2 say dtmod
EXTS = ('.elemod', '.dtmod')               # what the loader lists; the SDK writes .elemod


def format_of(doc):
    """-> the file's format number (1 or 2), from "elemod" or the legacy "dtmod"."""
    if not isinstance(doc, dict):
        return None
    return doc.get(KEY, doc.get(LEGACY_KEY))


def mod_files(folder):
    """-> the mod files in a folder, sorted: .elemod, and legacy .dtmod."""
    import glob
    out = []
    for ext in EXTS:
        out += glob.glob(os.path.join(folder, '*' + ext))
    return sorted(out)


class ModError(ValueError):
    pass


def sha(b):
    return hashlib.sha256(b).hexdigest()


def _int(v, what):
    if isinstance(v, int):
        return v
    try:
        return int(v, 0)
    except (TypeError, ValueError):
        raise ModError('%s: %r is not a number' % (what, v))


def _hex(v, what):
    try:
        return bytes.fromhex(v)
    except (TypeError, ValueError):
        raise ModError('%s: not hex' % what)


def resolve_target(doc, name):
    """-> (Device, Release) for a mod's "target", or ModError."""
    try:
        return devices.for_target(doc.get('target'))
    except devices.UnknownFirmware as e:
        raise ModError('%s: %s' % (name, e))


def parse_parts(parts, what, dev, rel):
    """[["hex", "..."] | ["stock", addr, n], ...] -> [('hex', bytes) | ('stock', addr, n)].
    A stock part is n bytes the patcher copies from the user's own
    (hash-checked) stock image: what a mod repeats from the firmware is never
    shipped."""
    lo, hi = dev.main_load, dev.image_end(rel)
    out = []
    for p in parts:
        if p and p[0] == 'hex' and len(p) == 2:
            out.append(('hex', _hex(p[1], what + ' part')))
        elif p and p[0] == 'stock' and len(p) == 3:
            a, k = _int(p[1], what + ' part'), _int(p[2], what + ' part')
            if a < lo or a + k > hi or k <= 0:
                raise ModError('%s: a part copies 0x%08x +%d, outside the image' % (what, a, k))
            out.append(('stock', a, k))
        else:
            raise ModError('%s: bad part %r' % (what, p))
    return out


def parts_bytes(parts, image, dev):
    out = bytearray()
    for p in parts:
        if p[0] == 'hex':
            out += p[1]
        else:
            o = p[1] - dev.main_load
            out += image[o:o + p[2]]
    return out


def parse_sites(doc, name, dev, rel, relocs=False):
    lo, hi = dev.main_load, dev.image_end(rel)
    out = []
    for i, s in enumerate(doc.get('sites', [])):
        what = '%s site %d' % (name, i)
        addr, n = _int(s.get('addr'), what), _int(s.get('len'), what)
        new = _hex(s.get('new'), what)
        if len(new) != n or n <= 0:
            raise ModError('%s: "new" is not %d bytes' % (what, n))
        if addr < lo or addr + n > hi:
            raise ModError('%s: 0x%08x +%d is outside the stock image' % (what, addr, n))
        if s.get('kind') not in ('code', 'data'):
            raise ModError('%s: kind is "code" or "data"' % what)
        h = s.get('stock_sha256')
        if not isinstance(h, str) or len(h) != 64:
            raise ModError('%s: no stock_sha256' % what)
        site = {'addr': addr, 'len': n, 'new': new, 'kind': s['kind'], 'stock_sha256': h.lower(),
                'relocs': []}
        if relocs:
            for r in s.get('relocs', []):
                off, typ, tgt, add = r
                off = _int(off, what)
                if typ not in ('abs32', 'pc32', 'pc16') or off < 0 \
                        or off + (2 if typ == 'pc16' else 4) > n:
                    raise ModError('%s: bad relocation %r' % (what, r))
                site['relocs'].append((off, typ, tgt, _int(add, what)))
        out.append(site)
    return out


def parse_resources(doc, name, dev):
    r = doc.get('resources') or {}
    regions = []
    for g in r.get('regions', []):
        lo, hi = _int(g.get('lo'), name + ' region'), _int(g.get('hi'), name + ' region')
        if not lo < hi:
            raise ModError('%s: empty region %r' % (name, g))
        area = [k for k, (a, z) in dev.areas.items() if a <= lo and hi <= z]
        if not area:
            raise ModError('%s: region %s 0x%08x-0x%08x is outside every free area'
                           % (name, g.get('name'), lo, hi))
        regions.append({'name': str(g.get('name', '?')), 'lo': lo, 'hi': hi, 'area': area[0]})
    return regions, [str(x) for x in r.get('names', [])]


def read_json(path):
    with open(path, 'rb') as fh:
        raw = fh.read()
    try:
        return json.loads(raw.decode('utf-8')), raw
    except (UnicodeDecodeError, json.JSONDecodeError) as e:
        raise ModError('%s: not JSON (%s)' % (path, e))


def load_any(path):
    """-> a format-1 Mod or a format-2 link.Mod2, by the file's "elemod"."""
    doc, raw = read_json(path)
    if format_of(doc) == 2:
        from . import link
        return link.Mod2(doc, os.path.basename(path), raw)
    return Mod(doc, os.path.basename(path), raw)


COMMON = {'elemod', 'dtmod', 'id', 'version', 'title', 'description', 'category', 'author', 'license',
          'target',
          'sites', 'resources', 'requires', 'conflicts', 'build', 'signature', 'notes'}


class Mod:
    """A format-1 mod: a whole build as one file (sites + one appended blob)."""

    def __init__(self, doc, name='<mod>', raw=None):
        self.doc, self.name = doc, name
        self.sha256 = sha(raw) if raw is not None else None
        if format_of(doc) != FORMAT:
            raise ModError('%s: not a format-%d .elemod file' % (name, FORMAT))
        for k in ('id', 'version', 'target', 'sites'):
            if k not in doc:
                raise ModError('%s: no "%s"' % (name, k))
        extra = set(doc) - COMMON - {'ele3_version', 'blob'}
        if extra:
            raise ModError('%s: unknown fields %s' % (name, sorted(extra)))
        self.id, self.version = str(doc['id']), str(doc['version'])
        self.dev, self.rel = resolve_target(doc, name)
        ev = doc.get('ele3_version')        # the version the unit shows (any device)
        if ev is not None:
            n = len(ev.encode('ascii', 'replace')) if isinstance(ev, str) else -1
            exact = self.dev.container == 'ele3'
            if (exact and n != self.dev.version_len) or not 0 < n <= self.dev.version_len:
                raise ModError('%s: ele3_version is %s %d ASCII characters on the %s'
                               % (name, 'exactly' if exact else 'up to', self.dev.version_len,
                                  self.dev.name))
        self.sites = parse_sites(doc, name, self.dev, self.rel)
        self.blob = None
        b = doc.get('blob')
        if b is not None:
            load, n = _int(b.get('load'), name + ' blob'), _int(b.get('len'), name + ' blob')
            parts = parse_parts(b.get('parts', []), name + ' blob', self.dev, self.rel)
            total = sum(len(p[1]) if p[0] == 'hex' else p[2] for p in parts)
            if total != n:
                raise ModError('%s: blob parts make %d bytes, not %d' % (name, total, n))
            room = self.dev.blob_max if self.dev.blob_max is not None else (
                self.dev.ddr[1] - self.dev.ddr[0] if self.dev.linkable() else None)
            if room is not None and n > room:
                raise ModError('%s: blob of %d bytes is over the %d the %s allows'
                               % (name, n, room, self.dev.name))
            self.blob = {'load': load, 'len': n, 'sha256': str(b.get('sha256', '')).lower(),
                         'parts': parts}
        self.regions, self.names = parse_resources(doc, name, self.dev)
        self.requires = [str(x) for x in doc.get('requires', [])]
        self.conflicts = [str(x) for x in doc.get('conflicts', [])]

    @classmethod
    def load(cls, path):
        doc, raw = read_json(path)
        return cls(doc, os.path.basename(path), raw)

    def label(self):
        return '%s %s' % (self.id, self.version)


# ---- instruction boundaries ---------------------------------------------------------

def insn_check(image, addr, n, dev, sweeps=32):
    """Does [addr, addr+n) of `image` (loaded at dev.main_load) hold whole
    instructions? -> (ok, note). The end is exact: decoding from addr must
    land on addr+n with no illegal word. The start cannot be proven by
    decoding forward, so `sweeps` linear sweeps, started 32 to 94 bytes
    before addr, must all land on it: the instruction stream resynchronises
    within a few words, so a sweep that steps over addr means addr may be
    mid-instruction. (Sweeps started only 2-4 bytes before can begin inside
    the previous instruction's extension words and say nothing: on Digitakt
    1.53 the known-good sites 0x400d52f0 and 0x40077e9a each fail one of
    those and none from further back.)"""
    isa = dev.decoder()
    read = isa.reader(image, dev.main_load)
    bad = isa.ILLEGAL | isa.LINEF | isa.FPU
    a = addr
    while a < addr + n:
        ins = isa.decode(read(a), a)
        if ins.flags & bad:
            return False, '0x%08x does not decode' % a
        a += ins.length
    if a != addr + n:
        return False, 'ends mid-instruction (0x%08x)' % a
    land = ran = 0
    for k in range(16, 16 + sweeps):
        pc = addr - 2 * k
        if pc < dev.main_load:
            break
        while pc < addr:
            pc += isa.decode(read(pc), pc).length
        ran += 1
        land += pc == addr
    if land != ran:
        return False, 'only %d of %d sweeps land on the start' % (land, ran)
    return True, 'whole instructions'


# ---- the static check, and apply for bundles ----------------------------------------

def overlaps(spans):
    """[(lo, hi, ...)] -> every pair that shares a byte."""
    spans = sorted(spans, key=lambda x: (x[0], x[1]))
    out = []
    for i, a in enumerate(spans):
        for b in spans[i + 1:]:
            if b[0] >= a[1]:
                break
            out.append((a, b))
    return out


def require_parts(r):
    """A `requires` entry: 'id' or 'id>=version' -> (id, the lowest version it takes, or None)."""
    r = str(r)
    if '>=' in r:
        rid, low = r.split('>=', 1)
        return rid.strip(), low.strip()
    return r.strip(), None


def version_key(v):
    """'0.2' -> a key that orders dotted numbers numerically (0.10 after 0.9); any other version
    orders as text, after every numeric one."""
    try:
        return (0,) + tuple(int(x) for x in str(v).split('.'))
    except ValueError:
        return (1, str(v))


def require_problem(m, r, by_id):
    """-> the problem with mod m's requirement r among the given mods {id: mod}, or None."""
    rid, low = require_parts(r)
    if rid not in by_id:
        return '%s requires %s' % (m.label(), r)
    if low is not None and version_key(by_id[rid].version) < version_key(low):
        return '%s requires %s %s or later, and %s is given' % (m.label(), rid, low, by_id[rid].label())
    return None


def claim_span(name):
    """A resource name that numbers what it claims: 'space:A' or 'space:A-B' (integers, 0x... or
    decimal; A-B inclusive) -> (space, A, B). Any other name -> None: it is matched exactly."""
    space, sep, rest = str(name).partition(':')
    if not sep or not space:
        return None
    lo, dash, hi = rest.partition('-')
    try:
        a = int(lo, 0)
        b = int(hi, 0) if dash else a
    except ValueError:
        return None
    return (space, a, b) if a <= b else None


def claim_problems(mods):
    """Two mods may not claim the same named resource; numbered claims in the same space
    ('dsp-reply:0x10-0x15' and 'dsp-reply:0x04-0x13') may not share a number either."""
    bad, owner, spans = [], {}, []
    for m in mods:
        for nm in m.names:
            if nm in owner and owner[nm] is not m:
                bad.append('%s and %s both claim %s' % (owner[nm].label(), m.label(), nm))
            owner[nm] = m
            sp = claim_span(nm)
            if sp is not None:
                spans.append((sp, nm, m))
    for i, (a, an, am) in enumerate(spans):
        for b, bn, bm in spans[i + 1:]:
            if am is not bm and an != bn and a[0] == b[0] and a[1] <= b[2] and b[1] <= a[2]:
                bad.append('%s claims %s and %s claims %s: both claim %s:0x%x-0x%x'
                           % (am.label(), an, bm.label(), bn, a[0], max(a[1], b[1]), min(a[2], b[2])))
    return bad


def same_target(mods):
    """-> problems if the mods were made for different firmware."""
    ts = {(m.dev.key, m.rel.version) for m in mods}
    if len(ts) > 1:
        return ['the mods are for different firmware: %s'
                % ', '.join('%s (%s %s)' % (m.label(), m.dev.name, m.rel.version) for m in mods)]
    return []


def common_checks(mods, image, spans):
    """The checks both formats share. `spans`: [(lo, hi, mod, what)] of image
    bytes each mod changes or claims. -> problems."""
    bad = same_target(mods)
    ids = [m.id for m in mods]
    for i in sorted(set(ids)):
        if ids.count(i) > 1:
            bad.append('mod %s is given %d times' % (i, ids.count(i)))
    by_id = {m.id: m for m in mods}
    for m in mods:
        for r in m.requires:
            p = require_problem(m, r, by_id)
            if p:
                bad.append(p)
        for c in m.conflicts:
            if c in ids:
                bad.append('%s conflicts with %s' % (m.label(), c))
    for a, b in overlaps(spans):
        if a[2] is b[2] and a[3] == b[3]:
            continue
        bad.append('%s %s and %s %s overlap (0x%08x-0x%08x)'
                   % (a[2].label(), a[3], b[2].label(), b[3], b[0], min(a[1], b[1])))
    bad += claim_problems(mods)
    if bad and bad[0].startswith('the mods are for different firmware'):
        return bad
    dev = mods[0].dev
    for m in mods:
        for s in m.sites:
            for lo, hi, why in dev.protected:
                if s['addr'] < hi and lo < s['addr'] + s['len']:
                    bad.append('%s site 0x%08x is inside 0x%08x-0x%08x, %s: protected on the %s'
                               % (m.label(), s['addr'], lo, hi, why, dev.name))
    for m in mods:
        for s in m.sites:
            o = s['addr'] - dev.main_load
            if sha(image[o:o + s['len']]) != s['stock_sha256']:
                bad.append('%s site 0x%08x: the stock bytes are not the ones it expects'
                           % (m.label(), s['addr']))
            elif s['kind'] == 'code':
                ok, note = insn_check(image, s['addr'], s['len'], dev)
                if not ok:
                    bad.append('%s site 0x%08x: %s' % (m.label(), s['addr'], note))
    return bad


FOLLOW_ON = ('imports ', 'adds to table ', 'has .fast code')


def summarize(problems, mods):
    """The checker's lines, for a person:
    - a mod that lacks a requirement shows that, not the unresolved names and
      tables that follow from it;
    - repeats are merged, with a count;
    - "no .boot" says that core is not enabled."""
    ids = {m.id for m in mods}
    lacking = {m.label() for m in mods if any(require_parts(r)[0] not in ids for r in m.requires)}
    out, count = [], {}
    for x in problems:
        if any(x.startswith(lab + ' ') and any(f in x for f in FOLLOW_ON) for lab in lacking):
            continue
        if x.startswith('exactly one mod must carry .boot') and x.endswith('got none'):
            x = 'core is not enabled, and every other mod builds on it'
        if x not in count:
            out.append(x)
        count[x] = count.get(x, 0) + 1
    return [x if count[x] == 1 else '%s (%d times)' % (x, count[x]) for x in out]


def check(mods, image):
    """The static conflict check for format-1 mods. -> problems."""
    spans = []
    for m in mods:
        for s in m.sites:
            spans.append((s['addr'], s['addr'] + s['len'], m, 'site 0x%08x' % s['addr']))
        if m.blob:
            spans.append((m.blob['load'], m.blob['load'] + m.blob['len'], m, 'blob'))
    bad = common_checks(mods, image, spans)
    regs = [(g['lo'], g['hi'], m, 'region ' + g['name']) for m in mods for g in m.regions]
    for a, b in overlaps(regs):
        bad.append('%s %s and %s %s overlap (0x%08x-0x%08x)'
                   % (a[2].label(), a[3], b[2].label(), b[3], b[0], min(a[1], b[1])))
    blobs = [m for m in mods if m.blob]
    if len(blobs) > 1:
        bad.append('more than one whole build (%s)' % ', '.join(m.label() for m in blobs))
    for m in blobs:
        end = m.dev.image_end(m.rel)
        if m.blob['load'] != end:
            bad.append('%s: its blob loads at 0x%08x, not the image end 0x%08x'
                       % (m.label(), m.blob['load'], end))
    return bad


def blob_bytes(mod, image):
    """-> the mod's blob, its stock parts copied from `image`."""
    out = parts_bytes(mod.blob['parts'], image, mod.dev)
    if sha(bytes(out)) != mod.blob['sha256']:
        raise ModError('%s: the blob does not assemble to its sha256' % mod.label())
    return bytes(out)


def apply(mods, image):
    """Format-1 mods -> the patched main OS image. Raises ModError with every
    problem the check finds; nothing is applied unless all of them combine."""
    rel = mods[0].rel
    if sha(image) != rel.main_sha256:
        raise ModError('the stock main OS is not %s %s' % (mods[0].dev.name, rel.version))
    bad = summarize(check(mods, image), mods)
    if bad:
        raise ModError('the mods do not combine:\n  ' + '\n  '.join(bad))
    dev = mods[0].dev
    img = bytearray(image)
    for m in mods:
        for s in m.sites:
            o = s['addr'] - dev.main_load
            img[o:o + s['len']] = s['new']
    for m in mods:
        if m.blob:
            img += blob_bytes(m, image)
    return bytes(img)
