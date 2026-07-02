#!/usr/bin/env python3
#
# Usage:
#   ./pmtable_decode.py                     # live kernel, socket 0
#   ./pmtable_decode.py --all               # include zero words
#   ./pmtable_decode.py --find 50:5         # find u32/f32/f64 ~= 50 (+/- 5)
#   ./pmtable_decode.py --find 1600         # find an exact-ish value (e.g. clk)
#   ./pmtable_decode.py --layout-scan       # find where the AVT field layout fits
#   ./pmtable_decode.py --layout --base 0x40  # overlay the layout at an offset
#   ./pmtable_decode.py --raw-dir /var/tmp  # also write raw table bytes per skt
#   ./pmtable_decode.py --target unix.0 vmcore.0    # decode from a crash dump

import argparse
import math
import os
import re
import struct
import subprocess
import sys

# Confirmed/worked-out fields, keyed by byte offset into the table. Each value
# is (name, kind, scale, unit) where kind is one of 'f32', 'u32', 'f64', 'u64'
# and the reported value is raw * scale.  Empty until we confirm offsets; e.g.
# once known:
#   0x000: ("socket_power", "f32", 1.0, "W"),
KNOWN_FIELDS = {
}

# Loose plausible ranges for quantities we expect, used only to tag candidate
# floats and help locate fields.  These overlap on purpose.
CANDIDATE_RANGES = (
    ("eff_freq_GHz", 0.2,   6.0),
    ("power_W",     1.0,    600.0),
    ("current_A",   10.0,   1000.0),
    ("fclk_MHz",    800.0,  2200.0),
    ("uclk/memclk", 600.0,  4000.0),
    ("temp_C",      15.0,   110.0),
    ("voltage_V",   0.2,    1.6),
)

# Each row: (name_fmt, count, lo, hi, unit); name_fmt takes the core index when
# it contains '%d'.  lo/hi is the plausible range used for the fit score.
MAX_CORES = 96

POWER_DATA_LAYOUT = (
    ("Core%d Effective Frequency", MAX_CORES, 0.2, 6.0,   "GHz"),
    ("Core%d C0 residency",        MAX_CORES, 0.0, 100.0, "%"),
    ("Core%d CC1 residency",       MAX_CORES, 0.0, 100.0, "%"),
    ("Core%d CC6 residency",       MAX_CORES, 0.0, 100.0, "%"),
    ("PC6 residency",              1,         0.0, 100.0, "%"),
    ("Package Power",              1,         1.0, 600.0, "W"),
)

# Total bytes one package block would occupy as packed f32.
LAYOUT_SPAN = sum(c for (_, c, _, _, _) in POWER_DATA_LAYOUT) * 4


def run_mdb(target, cmd):
    argv = ["mdb"] + target + ["-e", cmd]
    try:
        p = subprocess.run(argv, capture_output=True, text=True)
    except FileNotFoundError:
        sys.exit("mdb not found; run this on the target host.")
    if p.returncode != 0 and not p.stdout.strip():
        sys.stderr.write("mdb failed (%s):\n%s\n" %
                         (" ".join(argv), p.stderr.strip()))
    return p.stdout


def get_buffer_loc(target, sock, field="pmtable"):
    """Return (kva, length) for a socket's buffer, or (0, 0) if unregistered."""
    expr = ("zen_fabric::print zen_fabric_t "
            "zf_socs[%d].zs_iodies[0].zi_%s "
            "zf_socs[%d].zs_iodies[0].zi_%s_len" % (sock, field, sock, field))
    kva = length = 0
    for line in run_mdb(target, expr).splitlines():
        m = re.search(r'zi_%s\s*=\s*(0x[0-9a-fA-F]+)' % field, line)
        if m:
            kva = int(m.group(1), 16)
        m = re.search(r'zi_%s_len\s*=\s*(0x[0-9a-fA-F]+|\d+)' % field, line)
        if m:
            length = int(m.group(1), 0)
    return kva, length


def read_bytes(target, addr, length, phys=False):
    """Read `length` bytes at addr via mdb ::dump; return them as a bytes object.

    ::dump prints raw hex addresses (unlike /X, which mdb symbolizes), 16 bytes
    per line as four 8-hex-digit groups in memory order, then an ASCII column.
    With phys=True it uses ::dump -p to read a physical address, which works from
    live mdb -k and reaches firmware-reserved memory the kpm physmap excludes.
    """
    cmd = "0x%x,0t%d::dump%s" % (addr, length, " -p" if phys else "")
    out = run_mdb(target, cmd)
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r'^\s*[0-9a-fA-F]+:\s+(.*)$', line)
        if not m:
            continue
        groups = 0
        for tok in m.group(1).split():
            if groups < 4 and re.fullmatch(r'[0-9a-fA-F]{8}', tok):
                data += bytes.fromhex(tok)
                groups += 1
            else:
                break  # reached the ASCII column
    return bytes(data[:length])


def as_f32(data, off):
    return struct.unpack_from('<f', data, off)[0]


def as_u32(data, off):
    return struct.unpack_from('<I', data, off)[0]


def as_f64(data, off):
    return struct.unpack_from('<d', data, off)[0]


def as_u64(data, off):
    return struct.unpack_from('<Q', data, off)[0]


def sane_float(v):
    return math.isfinite(v) and (v == 0.0 or 1e-3 <= abs(v) <= 1e7)


def decode_grid(data, show_all):
    print("  offset      u32 hex     u32 dec           f32   candidates")
    print("  ------      -------     -------           ---   ----------")
    for off in range(0, len(data) - 3, 4):
        uv = as_u32(data, off)
        if uv == 0 and not show_all:
            continue
        fv = as_f32(data, off)
        fstr = "%.5g" % fv if sane_float(fv) else ""
        tags = [n for (n, lo, hi) in CANDIDATE_RANGES
                if sane_float(fv) and fv != 0.0 and lo <= fv <= hi]
        print("  0x%04x   %8x  %10u  %12s   %s" %
              (off, uv, uv, fstr, ",".join(tags)))


def decode_known(data):
    if not KNOWN_FIELDS:
        return
    print("\n  Decoded known fields:")
    conv = {"f32": as_f32, "u32": as_u32, "f64": as_f64, "u64": as_u64}
    for off in sorted(KNOWN_FIELDS):
        name, kind, scale, unit = KNOWN_FIELDS[off]
        if off + (8 if kind in ("f64", "u64") else 4) > len(data):
            continue
        val = conv[kind](data, off) * scale
        print("    0x%04x  %-28s %g %s" % (off, name, val, unit))


def find_values(data, specs):
    print("\n  Searches:")
    for spec in specs:
        if ':' in spec:
            vs, ts = spec.split(':', 1)
            val, tol = float(vs), float(ts)
        else:
            val = float(spec)
            tol = abs(val) * 0.02 + 1e-6
        hits = []
        for off in range(0, len(data) - 3, 4):
            fv = as_f32(data, off)
            if math.isfinite(fv) and abs(fv - val) <= tol:
                hits.append((off, "f32", fv))
            uv = as_u32(data, off)
            if abs(uv - val) <= tol:
                hits.append((off, "u32", float(uv)))
        for off in range(0, len(data) - 7, 4):
            dv = as_f64(data, off)
            if math.isfinite(dv) and abs(dv - val) <= tol:
                hits.append((off, "f64", dv))
        print("    %g (+/- %g):" % (val, tol))
        for off, kind, got in hits:
            print("      0x%04x  %s = %g" % (off, kind, got))
        if not hits:
            print("      (no match)")


def layout_fields(base):
    """Yield (offset, name, lo, hi, unit) for the hypothesised layout at base."""
    off = base
    for (fmt, count, lo, hi, unit) in POWER_DATA_LAYOUT:
        for i in range(count):
            name = (fmt % i) if "%d" in fmt else fmt
            yield (off, name, lo, hi, unit)
            off += 4


def layout_fit(data, base):
    """Count layout fields whose f32 is non-zero and in range, overlaid at base."""
    good = total = 0
    for (off, _name, lo, hi, _unit) in layout_fields(base):
        if off + 4 > len(data):
            break
        total += 1
        v = as_f32(data, off)
        if math.isfinite(v) and v != 0.0 and lo <= v <= hi:
            good += 1
    return good, total


def layout_scan(data):
    """Try every 4-byte-aligned base and report the best-fitting ones.

    A random or empty buffer scores near zero; the real layout lights up because
    frequencies land in GHz, residencies in 0..100 and package power in watts.
    """
    print("\n  Layout fit scan (best bases; non-zero in-range / total):")
    last = len(data) - LAYOUT_SPAN
    if last < 0:
        print("    buffer (%d bytes) smaller than one layout block (%d)" %
              (len(data), LAYOUT_SPAN))
        return
    results = []
    for base in range(0, last + 1, 4):
        good, total = layout_fit(data, base)
        if good:
            results.append((good / total, good, total, base))
    if not results:
        print("    no base produced non-zero in-range values "
              "(table empty or layout wrong)")
        return
    results.sort(reverse=True)
    for frac, good, total, base in results[:8]:
        print("    base 0x%04x   %3d/%-3d  (%.0f%%)" %
              (base, good, total, 100 * frac))


def decode_layout(data, base, full):
    print("\n  Layout overlay (hypothesis from pmm-nda.utp) at base 0x%x:" % base)
    off = base
    tot_good = tot = 0
    for (fmt, count, lo, hi, unit) in POWER_DATA_LAYOUT:
        vals = []
        start = off
        for _ in range(count):
            if off + 4 > len(data):
                break
            vals.append(as_f32(data, off))
            off += 4
        nz = [v for v in vals if math.isfinite(v) and v != 0.0]
        good = sum(1 for v in nz if lo <= v <= hi)
        tot_good += good
        tot += len(vals)
        label = re.sub(r"%d", "*", fmt)
        rng = ("min=%.4g max=%.4g" % (min(nz), max(nz))) if nz else "all zero"
        print("    0x%04x  %-30s x%-3d  in-range %3d/%-3d  %s" %
              (start, "%s (%s)" % (label, unit), count, good, len(vals), rng))
        if full:
            for j, v in enumerate(vals):
                nm = (fmt % j) if "%d" in fmt else fmt
                print("        0x%04x  %-30s %.5g %s" %
                      (start + j * 4, nm, v, unit))
    if tot:
        print("    fit: %d/%d non-zero values in range (%.0f%%)" %
              (tot_good, tot, 100 * tot_good / tot))


def main():
    ap = argparse.ArgumentParser(
        description="Extract and decode the Milan SMU PM table via mdb.")
    ap.add_argument("--target", nargs="+", default=["-k"], metavar="ARG",
                    help="mdb target args (default: -k, the live kernel)")
    ap.add_argument("-s", "--socket", type=int, action="append",
                    help="socket(s) to decode (default: socket 0)")
    ap.add_argument("--all", action="store_true",
                    help="show zero-valued words too")
    ap.add_argument("--find", action="append", default=[], metavar="VAL[:TOL]",
                    help="search the table for a value you already know")
    ap.add_argument("--layout-scan", action="store_true",
                    help="scan for the base offset where the AVT layout fits")
    ap.add_argument("--layout", action="store_true",
                    help="overlay the pmm-nda.utp field layout (a hypothesis)")
    ap.add_argument("--layout-full", action="store_true",
                    help="with --layout, print every individual field")
    ap.add_argument("--base", type=lambda s: int(s, 0), default=0, metavar="OFF",
                    help="base offset for --layout (default 0)")
    ap.add_argument("--raw-dir", metavar="DIR",
                    help="also write raw table bytes to DIR/pmtable_sockN.bin")
    ap.add_argument("--dbg", action="store_true",
                    help="read the 0x7 debug buffer (zi_pmdbg) instead")
    ap.add_argument("--phys", metavar="PA[:LEN]",
                    help="read a physical range via the kpm physmap")
    ap.add_argument("--max-bytes", type=lambda s: int(s, 0), default=0x10000,
                    metavar="N", help="cap bytes read (default 64KiB)")
    args = ap.parse_args()

    if args.phys:
        spec = args.phys.split(":", 1)
        pa = int(spec[0], 0)
        plen = min(int(spec[1], 0) if len(spec) > 1 else args.max_bytes,
                   args.max_bytes)
        data = read_bytes(args.target, pa, plen, phys=True)
        print("=== phys 0x%x, len %d (read %d bytes) ===" %
              (pa, plen, len(data)))
        decode_grid(data, args.all)
        if args.find:
            find_values(data, args.find)
        if args.layout_scan:
            layout_scan(data)
        return

    field = "pmdbg" if args.dbg else "pmtable"

    # Gimlet is single-socket, single-IO-die, so default to socket 0; -s
    # overrides in the unlikely event this is run on a multi-socket board.
    sockets = args.socket if args.socket else [0]
    for s in sockets:
        kva, length = get_buffer_loc(args.target, s, field)
        if kva == 0 or length == 0:
            print("=== socket %d: no %s buffer registered ===" % (s, field))
            continue
        length = min(length, args.max_bytes)
        data = read_bytes(args.target, kva, length)
        print("=== socket %d: %s kva 0x%x, len %d (read %d bytes) ===" %
              (s, field, kva, length, len(data)))
        if args.raw_dir:
            path = os.path.join(args.raw_dir, "%s_sock%d.bin" % (field, s))
            with open(path, "wb") as f:
                f.write(data)
            print("  wrote raw table to %s" % path)
        decode_grid(data, args.all)
        decode_known(data)
        if args.find:
            find_values(data, args.find)
        if args.layout_scan:
            layout_scan(data)
        if args.layout or args.layout_full:
            decode_layout(data, args.base, args.layout_full)
        print()


if __name__ == "__main__":
    main()
