#!/usr/bin/env python3
"""
Function-boundary finder for Turok: Rage Wars (USA).

Combines several independent signals into one list of function starts:
  - splat/spimdisasm's own glabels (passed in as text files of addresses)
  - n64sym library matches (passed in as splat-format "name = 0xADDR;" files)
  - every `jal` target in the code segments
  - code addresses built with lui/addiu (or lui/ori) pairs, and 32-bit words
    in the data segments that point at code, when the word before the target
    ends a function (jr $ra + delay slot, or a `j` + delay slot) - this keeps
    switch-case labels out
  - "after jr $ra + delay slot, skipping nop padding, a new function begins"
    when the next instruction is a stack-frame setup (addiu $sp,$sp,-N)

The game's code is TLB-mapped (see docs/ROM_LAYOUT.md):
  main  ROM 0x001000-0x0C24F0  vram 0x00200400-0x002C18F0
  ovl   ROM 0x164000-0x1B22B4  vram 0x00400000-0x0044E2B4

Usage:
  find_functions.py <rom.z64> <out.txt> [--glabels FILE ...] [--n64sym FILE ...]
Writes "0xADDR size name" lines, sorted, one per function.
"""
import argparse
import re
import struct

SEGMENTS = [
    # name, rom_start, rom_text_end, vram_start, rom_data_end
    ("main", 0x001000, 0x0C24F0, 0x00200400, 0x101000),
    ("ovl",  0x164000, 0x1B22B4, 0x00400000, 0x1B5A50),
]

JR_RA = 0x03E00008


def word(rom, off):
    return struct.unpack_from(">I", rom, off)[0]


def seg_of(vram):
    for s in SEGMENTS:
        name, rs, re_, vs, _ = s
        if vs <= vram < vs + (re_ - rs):
            return s
    return None


def rom_of(vram):
    s = seg_of(vram)
    return None if s is None else s[1] + (vram - s[3])


def ends_function(rom, vram, after_data=False):
    """True if the two instructions before `vram` end a function (or, with
    after_data, if data sits right before it)."""
    off = rom_of(vram)
    if off is None:
        return False
    s = seg_of(vram)
    # skip back over nop padding
    while off - 8 >= s[1] and word(rom, off - 4) == 0 and word(rom, off - 8) == 0:
        off -= 4
    if off - 8 < s[1]:
        return off == s[1]
    w = word(rom, off - 8)
    if w == JR_RA:
        return True
    if (w >> 26) == 2:  # j
        return True
    # `b` (beq $zero,$zero) as an unconditional tail
    if (w >> 16) == 0x1000:
        return True
    if not after_data:
        return False
    # data (string fragments) right before it
    import rabbitizer
    prev = word(rom, off - 4)
    if not rabbitizer.Instruction(prev).isValid():
        return True
    text = rom[off - 8:off]
    if sum(0x20 <= b < 0x7F for b in text) >= 5:
        return True
    return False


def rom_of_data(rom, vram):
    """ROM offset of a data address (kseg0 or TLB-mapped), or None."""
    for name, rs, re_, vs, rde in SEGMENTS:
        if vs <= vram < vs + (rde - rs):
            return rs + (vram - vs)
    if 0x80000400 <= vram < 0x80100400:      # kseg0 view of the first MB
        return 0x1000 + (vram - 0x80000400)
    return None


def jump_table_targets(rom):
    """Entries of every jump table the code indexes and then `jr`s through.

    A linear constant propagation over each segment: lui/addiu/ori build
    constants, `addu` of a constant and an unknown is "constant + index",
    a `lw` from that is "entry of the table at constant + offset", and a
    `jr` of such an entry names the table. The table base often sits in a
    saved register set up long before the jr, so values survive until
    the register is overwritten or the function returns."""
    targets = set()
    tables = []
    for name, rs, re_, vs, rde in SEGMENTS:
        reg = {}
        for off in range(rs, re_, 4):
            w = word(rom, off)
            op = w >> 26
            r_s, r_t, r_d = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
            imm = w & 0xFFFF
            simm = imm - 0x10000 if imm & 0x8000 else imm
            if w == JR_RA:
                reg = {}
                continue
            if op == 0 and (w & 0x3F) == 0x08:          # jr rX
                v = reg.get(r_s)
                if isinstance(v, tuple) and v[0] == "entry":
                    tables.append((vs + off - rs, v[1]))
                continue
            if op == 0x0F:                               # lui
                reg[r_t] = (imm << 16) & 0xFFFFFFFF
            elif op == 0x09:                             # addiu
                v = reg.get(r_s)
                if isinstance(v, int):
                    reg[r_t] = (v + simm) & 0xFFFFFFFF
                elif isinstance(v, tuple) and v[0] == "index":
                    reg[r_t] = ("index", (v[1] + simm) & 0xFFFFFFFF)
                else:
                    reg.pop(r_t, None)
            elif op == 0x0D:                             # ori
                v = reg.get(r_s)
                if isinstance(v, int):
                    reg[r_t] = v | imm
                else:
                    reg.pop(r_t, None)
            elif op == 0x23:                             # lw
                v = reg.get(r_s)
                if isinstance(v, int):
                    reg[r_t] = ("entry", (v + simm) & 0xFFFFFFFF)
                elif isinstance(v, tuple) and v[0] == "index":
                    reg[r_t] = ("entry", (v[1] + simm) & 0xFFFFFFFF)
                else:
                    reg.pop(r_t, None)
            elif op == 0 and (w & 0x3F) in (0x21, 0x20):  # addu / add
                a, b = reg.get(r_s), reg.get(r_t)
                if isinstance(a, int) and b is None and r_t != 0:
                    reg[r_d] = ("index", a)
                elif isinstance(b, int) and a is None and r_s != 0:
                    reg[r_d] = ("index", b)
                elif r_t == 0 and a is not None:
                    reg[r_d] = a                          # move
                elif r_s == 0 and b is not None:
                    reg[r_d] = b
                else:
                    reg.pop(r_d, None)
            elif op == 0:
                reg.pop(r_d, None)
            elif op in (0x20, 0x21, 0x24, 0x25, 0x0A, 0x0B, 0x0C, 0x0E, 0x08):
                reg.pop(r_t, None)
    by_jr = {}
    for jr, base in tables:
        t = rom_of_data(rom, base)
        if t is None:
            continue
        n = 0
        while n < 512 and t + 4 * n + 4 <= len(rom):
            e = word(rom, t + 4 * n)
            if seg_of(e) is None:
                break
            targets.add(e)
            by_jr.setdefault(jr, []).append(e)
            n += 1
    return targets, by_jr


def branch_target(w, v):
    """Target of a conditional/unconditional branch at v, or None."""
    op = w >> 26
    off = ((w & 0xFFFF) ^ 0x8000) - 0x8000
    if op in (0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17):  # beq bne blez bgtz (+likely)
        return v + 4 + (off << 2)
    if op == 0x01 and ((w >> 16) & 0x1F) in (0, 1, 2, 3, 0x10, 0x11, 0x12, 0x13):  # regimm
        return v + 4 + (off << 2)
    if op == 0x11 and ((w >> 21) & 0x1F) == 0x08:  # bc1f/bc1t(l)
        return v + 4 + (off << 2)
    return None


def has_exit(rom, start, end):
    """True if the last instruction pair of [start, end) is an exit."""
    if end - start < 8:
        return False
    s = seg_of(start)
    w = word(rom, s[1] + (end - 8 - s[3]))
    return w == JR_RA or (w >> 26) == 2 or (w >> 16) == 0x1000


def function_end(rom, start, limit, jtables):
    """End of the function at `start`: past the delay slot of the first
    exit (jr $ra, a tail-call j, or an unconditional b) that no branch,
    jump or jump-table entry seen so far reaches beyond."""
    s = seg_of(start)
    furthest = start
    for v in range(start, limit, 4):
        w = word(rom, s[1] + (v - s[3]))
        op = w >> 26
        t = branch_target(w, v)
        if t is not None and start <= t < limit:
            furthest = max(furthest, t)
        if op == 2:
            t = (v & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            if start <= t < limit:
                furthest = max(furthest, t)
                if v + 8 > furthest:
                    return v + 8
                continue
            return v + 8 if v + 8 > furthest else limit  # tail call
        if op == 0 and (w & 0x3F) == 0x08:
            r = (w >> 21) & 31
            if r != 31:
                for e in jtables.get(v, ()):
                    if start <= e < limit:
                        furthest = max(furthest, e)
                continue
            if v + 8 > furthest:
                return v + 8
        if (w >> 16) == 0x1000 and v + 8 > furthest:  # b
            return v + 8
    return limit


def looks_like_code(rom, start, end):
    """Reject "functions" that are really data left in the text section
    (this game's text has string fragments between some functions)."""
    import rabbitizer
    s = seg_of(start)
    for v in range(start, end, 4):
        w = word(rom, s[1] + (v - s[3]))
        ins = rabbitizer.Instruction(w, vram=v)
        if not ins.isValid():
            return False
        # compilers never write $zero except as the canonical nop
        if not ins.isNop() and ((ins.modifiesRd() and ins.rd.value == 0)
                                or (ins.modifiesRt() and ins.rt.value == 0)):
            return False
        if (w >> 26) in (2, 3):
            t = ((v + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2)
            if seg_of(t) is None:
                return False
        t = branch_target(w, v)
        if t is not None and seg_of(t) is None:
            return False
        # $k0/$k1 belong to exception handlers, which are all named and
        # handled in the config; anywhere else they mean data
        op = w >> 26
        if op in (0x10, 0x11, 0x12):          # coprocessor ops: fields aren't GPRs
            gprs = ()
        elif op in (0x31, 0x35, 0x39, 0x3D):  # lwc1/ldc1/swc1/sdc1: rt is an FPR
            gprs = ((w >> 21) & 31,)
        elif op == 0:
            gprs = ((w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31)
        else:
            gprs = ((w >> 21) & 31, (w >> 16) & 31)
        if any(r in (26, 27) for r in gprs):
            return False
    return True


def skip_leading_junk(rom, start, limit):
    """A stray data word right before a stack-frame setup: start after it."""
    s = seg_of(start)
    w0 = word(rom, s[1] + (start - s[3]))
    if start + 4 <= limit and not is_frame_setup(w0) and \
            is_frame_setup(word(rom, s[1] + (start + 4 - s[3]))):
        t = branch_target(w0, start)
        if (t is not None and not (start < t < limit)) or (w0 >> 26) in (2, 3):
            return start + 4
    return start


def is_frame_setup(w):
    # addiu $sp, $sp, -N
    return (w >> 16) == 0x27BD and (w & 0x8000)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("out")
    ap.add_argument("--glabels", nargs="*", default=[])
    ap.add_argument("--n64sym", nargs="*", default=[])
    ap.add_argument("--extra", nargs="*", default=[],
                    help="files of 'name = 0xADDR;' to force in (e.g. hand-identified libultra)")
    args = ap.parse_args()
    rom = open(args.rom, "rb").read()

    starts = {}   # vram -> (name or None, source)
    data_targets = set()  # every data word that points into code
    jt_targets, jt_tables = jump_table_targets(rom)
    print(f"{len(jt_tables)} jump tables, {len(jt_targets)} case labels excluded")

    def add(v, src, name=None, force=False):
        if seg_of(v) is None or v & 3:
            return
        if v in starts:
            if name and not starts[v][0]:
                starts[v] = (name, starts[v][1] + "," + src)
            return
        if not force and src != "jal" and v in jt_targets:
            return
        # Data fragments sit in front of some functions, so a start that
        # follows data is accepted on stronger evidence: a pointer built in
        # code, or a target that opens with a stack-frame setup.
        if not force and src in ("ptr", "data") and not ends_function(
                rom, v, after_data=(src == "ptr" or is_frame_setup(word(rom, rom_of(v))))):
            return
        starts[v] = (name, src)

    for s in SEGMENTS:
        add(s[3], "segstart")

    for f in args.glabels:
        for line in open(f):
            m = re.search(r"([0-9A-Fa-f]{8})\s*$", line.strip())
            if line.strip().startswith("0x"):
                add(int(line.strip().split()[0], 16), "spim")
            elif m:
                add(int(m.group(1), 16), "spim")
    for path, src in [(p, "n64sym") for p in args.n64sym] + [(p, "extra") for p in args.extra]:
        for line in open(path):
            m = re.match(r"\s*(\S+)\s*=\s*(0x[0-9A-Fa-f]+)", line)
            if m:
                add(int(m.group(2), 16), src, m.group(1), force=True)

    for name, rs, re_, vs, rde in SEGMENTS:
        lui = {}
        for off in range(rs, re_, 4):
            w = word(rom, off)
            op = w >> 26
            if op == 3:  # jal
                pc = vs + (off - rs)
                add(((pc + 4) & 0xF0000000) | ((w & 0x03FFFFFF) << 2), "jal")
            elif op == 0x0F:  # lui
                lui[(w >> 16) & 31] = (w & 0xFFFF) << 16
            elif op in (0x09, 0x0D):  # addiu / ori
                rs_ = (w >> 21) & 31
                if rs_ in lui:
                    imm = w & 0xFFFF
                    if op == 0x09 and imm & 0x8000:
                        imm -= 0x10000
                    add((lui[rs_] + imm) & 0xFFFFFFFF, "ptr")
            # forget lui values across function ends
            if w == JR_RA:
                lui = {}
        # data words pointing at code
        for off in range(re_, rde, 4):
            add(word(rom, off), "data")
            data_targets.add(word(rom, off))
        # frame setup right after a function end
        for off in range(rs + 8, re_, 4):
            v = vs + (off - rs)
            w = word(rom, off)
            if is_frame_setup(w) and ends_function(rom, v, after_data=True):
                add(v, "frame")

    def resolve():
        """Turn the start set into (start, end, name, source) records."""
        out = []
        dropped = 0
        addrs = sorted(starts)
        for i, a in enumerate(addrs):
            s = seg_of(a)
            seg_end = s[3] + (s[2] - s[1])
            nxt = addrs[i + 1] if i + 1 < len(addrs) and addrs[i + 1] < seg_end else seg_end
            name, src = starts[a]
            if "extra" not in src:
                a = skip_leading_junk(rom, a, nxt)
                if a >= nxt:
                    continue  # only the junk word; the function is listed at nxt
                if name is None:
                    name = f"func_{a:08X}"
            end = function_end(rom, a, nxt, jt_tables)
            if src.split(",")[0] not in ("extra", "segstart") and "extra" not in src \
                    and not looks_like_code(rom, a, end):
                # Data in front of a real function: the first later word that
                # reads as a whole function is where it starts.
                window = range(a + 4, min(a + 0x40, nxt), 4)
                framed = [p for p in window
                          if is_frame_setup(word(rom, seg_of(p)[1] + (p - seg_of(p)[3])))]
                for p in framed + [p for p in window if p not in framed]:
                    pend = function_end(rom, p, nxt, jt_tables)
                    if (p in framed or has_exit(rom, p, pend)) and looks_like_code(rom, p, pend):
                        print(f"  0x{a:08X} ({src}): data, function starts at 0x{p:08X}")
                        a, end, name = p, pend, f"func_{p:08X}"
                        break
                else:
                    dropped += 1
                    print(f"  dropped 0x{a:08X} ({src}): not code")
                    continue
            out.append((a, end, nxt, name or f"func_{a:08X}", src))
        return out, dropped

    funcs, dropped = resolve()

    # Gaps: code after a function's end that nothing above found, usually a
    # function reached only through a pointer the scans missed, behind a data
    # fragment. In a gap that reads as a whole function (valid code ending in
    # an exit), a start is a stack-frame setup, or a target of a data word
    # right after data (a leaf function behind a string fragment, reached
    # only through a table). Being in a gap, neither can split a function.
    found = 0
    for a, end, nxt, name, src in funcs:
        p = end
        while p < nxt:
            w = word(rom, seg_of(p)[1] + (p - seg_of(p)[3]))
            if p not in jt_targets and (is_frame_setup(w) or (
                    p in data_targets and ends_function(rom, p, after_data=True))):
                pend = function_end(rom, p, nxt, jt_tables)
                if has_exit(rom, p, pend) and looks_like_code(rom, p, pend):
                    starts[p] = (None, "gap")
                    found += 1
                    p = pend
                    continue
            p += 4
    if found:
        print(f"  {found} functions found in gaps")
        funcs, dropped = resolve()

    with open(args.out, "w") as fo:
        for a, end, nxt, name, src in funcs:
            fo.write(f"0x{a:08X} 0x{end - a:X} {name} {src}\n")
    print(f"{len(funcs)} functions ({dropped} data fragments dropped)")

if __name__ == "__main__":
    main()
