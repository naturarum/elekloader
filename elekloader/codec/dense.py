# SPDX-License-Identifier: GPL-2.0-or-later
"""A denser encoder for the ELE3 sections' codec (the one codec/elz.py
decodes), with an optional in-place floor.

codec/aplib.py, digikit's encoder, is greedy: it takes the longest match it
finds, needs three bytes, and never writes the decoder's last-offset code.
That is enough on most devices. On the Digitone II its main OS stream is
260 KB longer than Elektron's, and the container no longer fits the 2 MiB the
OS receives an upgrade into (devices/digitone_mk2.py). This encoder writes
the same format, for the same decoder, more tightly:
- matches from two bytes (the shortest the format codes), up to 1 MiB back,
  found through a hash chain on their first two bytes;
- the last-offset code (gamma 2, no offset byte) wherever it pays;
- each choice by its cost in bits under the decoder's own coding, with one
  step of lazy evaluation: a match waits a literal when the next position's
  match saves more than that literal costs.

The floor: a loader that unpacks in place (the stream staged at `stage`, the
output written from `main_load` up over it) needs the writer to stay behind
the next stream byte it has not read. With base = stage - main_load,
pack_section keeps that distance above `reserve` bytes at every literal and
match, measured as syx.inplace_depack measures it. A match that would bring
it to `reserve` or below is written as literals instead; literals never
shrink the distance, they only lengthen the stream. The distance is
predicted before each match and checked against the stream as written, so a
wrong prediction raises instead of returning a stream that fails.

The device's depacker (0x80000432 in the Digitone II's bootstrap; the
copies in its section 4 and main OS are the same bytes) keeps no state but
its tag register and the last offset, so the last-offset code is valid after
a match as after a literal; and it reads the stream one byte at a time, with
no read-ahead, which is what the floor measures.

Every stream round-trips through codec/elz.py (tests/test_units.py).
"""
import struct

from .elz import BIAS, FAR, REUSE

CHAIN = 16              # candidates looked at per position
MAX_OFFSET = 1 << 20    # further back only makes the gamma longer
MAX_COUNT = 1 << 12     # bytes per match, as codec/aplib.py
GOOD = 32               # a match this long is taken without the lazy look
END_GAMMA = (1 << 24) + 2   # the end marker: (END_GAMMA << 8) + 0xFF == BIAS mod 2**32


class FloorError(ValueError):
    pass


def _gamma_bits(v):
    return 2 * (v.bit_length() - 1)


def _match_tagbits(off, count, last):
    """-> the tag bits one match takes (its offset byte is not one of them)."""
    b = 1 + (2 if off == last else _gamma_bits((off + BIAS) >> 8))
    lb = count - 1 - (1 if off > FAR else 0)
    return b + (2 if lb <= 3 else 2 + _gamma_bits(lb - 2))


def _cost(off, count, last):
    """-> a match's cost in bits, its offset byte included."""
    return _match_tagbits(off, count, last) + (0 if off == last else 8)


class _Writer:
    """Tag bits MSB first, each tag byte placed where the decoder fetches it,
    payload bytes after it in order: the layout codec/aplib.py's _assemble
    and elektron-firmware-tool write."""

    def __init__(self):
        self.o = bytearray()
        self.tag = 0
        self.left = 0

    def bit(self, v):
        if self.left == 0:
            self.tag = len(self.o)
            self.o.append(0)
            self.left = 8
        self.left -= 1
        if v:
            self.o[self.tag] |= 1 << self.left

    def gamma(self, v):
        for i in range(v.bit_length() - 2, -1, -1):
            self.bit((v >> i) & 1)
            self.bit(i == 0)

    def match(self, off, count, last):
        self.bit(0)
        if off == last:
            self.gamma(REUSE)
        else:
            raw = off + BIAS
            self.gamma(raw >> 8)
            self.o.append(raw & 0xFF)
        lb = count - 1 - (1 if off > FAR else 0)
        if lb <= 3:
            self.bit(lb >> 1)
            self.bit(lb & 1)
        else:
            self.bit(0)
            self.bit(0)
            self.gamma(lb - 2)

    def end(self):
        self.bit(0)
        self.gamma(END_GAMMA)
        self.o.append(0xFF)

    def after(self, tagbits, payload):
        """-> the stream's length once tagbits more tag bits and payload more
        bytes are written: where the decoder's next unread byte will be."""
        more = tagbits - self.left
        return len(self.o) + (0 if more <= 0 else (more + 7) // 8) + payload


def pack(data, base=None, reserve=0):
    """-> (stream without the section header, the smallest in-place distance
    or None without a floor). See the module docstring."""
    data = bytes(data)
    n = len(data)
    w = _Writer()
    head = {}
    prev = [-1] * n
    floor = None if base is None else base + 8      # the stream follows its 8-byte header
    least = None

    def insert(p):
        if p + 1 < n:
            k = (data[p] << 8) | data[p + 1]
            prev[p] = head.get(k, -1)
            head[k] = p

    def run(a, b, cap):
        k = 0
        while k < cap and data[a + k] == data[b + k]:
            k += 1
        return k

    def best(p, last):
        """-> (bits saved against literals, offset, count) of the best match at p."""
        cap = min(MAX_COUNT, n - p)
        if cap < 2:
            return 0, 0, 0
        bs = bo = bc = 0
        if p >= last:
            k = run(p - last, p, cap)
            if k >= (3 if last > FAR else 2):
                s = 9 * k - _cost(last, k, last)
                if s > 0:
                    bs, bo, bc = s, last, k
        j = head.get((data[p] << 8) | data[p + 1], -1)
        c, longest = CHAIN, 1
        while j >= 0 and c:
            c -= 1
            off = p - j
            if off > MAX_OFFSET:
                break
            if longest < cap and data[j + longest] == data[p + longest]:
                k = run(j, p, cap)
                if k > longest:
                    longest = k
                    if k >= (3 if off > FAR else 2):
                        s = 9 * k - _cost(off, k, last)
                        if s > bs:
                            bs, bo, bc = s, off, k
                    if k >= cap:
                        break
            j = prev[j]
        return bs, bo, bc

    i, last = 0, 1                          # the decoder starts with last offset 1
    while i < n:
        s, off, count = best(i, last)
        insert(i)
        if s > 0 and count < GOOD and i + 1 < n and best(i + 1, last)[0] > s + 9:
            s = 0                                       # a literal, then the better match
        if s > 0 and floor is not None:
            at = w.after(_match_tagbits(off, count, last), 0 if off == last else 1)
            # a literal after the match can sit one byte closer than the match does
            if floor + at - (i + count - 1) - 1 <= reserve:
                s = 0                                   # the floor: literals instead
        if s > 0:
            if floor is not None:
                want = floor + w.after(_match_tagbits(off, count, last),
                                       0 if off == last else 1) - (i + count - 1)
            w.match(off, count, last)
            if floor is not None:
                g = floor + len(w.o) - (i + count - 1)
                if g != want:
                    raise FloorError('the in-place distance after the match at %d is %d, not %d'
                                     % (i, g, want))
                least = g if least is None else min(least, g)
            last = off
            for p in range(i + 1, i + count):
                insert(p)
            i += count
        else:
            w.bit(1)
            if floor is not None:
                g = floor + len(w.o) - i
                if g <= reserve:
                    raise FloorError('the in-place distance at the literal %d is %d' % (i, g))
                least = g if least is None else min(least, g)
            w.o.append(data[i])
            i += 1
    w.end()
    return bytes(w.o), least


def pack_section(data, base=None, reserve=0):
    """pack(data) with the section's 8-byte header ([u32 stream length]
    [u32 byte sum], big-endian), as codec/aplib.py's pack_section writes it.
    -> (section, the smallest in-place distance or None)."""
    body, least = pack(data, base, reserve)
    return struct.pack('>II', len(body), sum(body) & 0xFFFFFFFF) + body, least
