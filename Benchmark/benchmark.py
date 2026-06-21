"""
benchmark.py
==========================
Baseline so sánh với BPAB — calibrated theo omnetpp.ini + BpabMac.cpp defaults.

Tham số lấy từ:
  slotDuration  = 0.0005  (dòng bị comment trong .ini)
  maxIterations = 4
  rangeR        = 400 m
  widthW        = 30 m   → halfWidth = 15 m (filter dải đường)
  minProgress   = 20 m   (node phải tiến về phía trước ít nhất 20m)

Thời gian 1 hop BPAB (để normalize delay Flooding/Weighted-P):
  timer3_timeout = slotDuration × (2×maxIterations + 20)
               = 0.0005 × (8 + 20) = 0.014 s/hop

Dùng:
  python benchmark.py
  python benchmark.py --log den5.txt --time 29.8 --source 80
"""

import re
import math
import random
import argparse
import sys
from collections import defaultdict

# ─────────────────────────────────────────────
# CONFIG — khớp với omnetpp.ini + C++ defaults
# ─────────────────────────────────────────────
DEFAULT_LOG    = "den5.txt"
DEFAULT_TIME   = 29.8
DEFAULT_SOURCE = 80

# Từ .ini (dòng commented) và C++ defaults
RANGE_R       = 400.0    # par("rangeR")        — bán kính tối đa
WIDTH_W       = 30.0     # par("widthW")         — bề rộng dải đường
MIN_PROGRESS  = 20.0     # par("minProgress")    — tiến tối thiểu
SLOT_DUR      = 0.0005   # par("slotDuration")   — 0.5 ms/slot
MAX_ITER      = 4        # par("maxIterations")
NUM_SLOTS     = 10       # dùng cho Slotted-P

# Thời gian 1 hop BPAB = timeout timer3 = slotDuration × (2×maxIterations + 20)
# timer 3: setTimer(3, slotDuration * (2 * maxIterations + 20))
HOP_DURATION_S = SLOT_DUR * (2 * MAX_ITER + 20)   # = 0.0005 × 28 = 0.014 s

MC_RUNS = 500
random.seed(42)

# ─────────────────────────────────────────────
# PARSE LOG
# ─────────────────────────────────────────────
_POS_RE = re.compile(
    r'(\d+(?:\.\d+)?)'
    r'.*?Node:(\d+)'
    r'.*?x:([-\d.]+)'
    r'.*?y:([-\d.]+)'
)

def load_positions(filename):
    snapshots = defaultdict(dict)
    try:
        with open(filename, "r") as f:
            for line in f:
                if "EVENT:POS" not in line:
                    continue
                m = _POS_RE.search(line)
                if not m:
                    continue
                snapshots[float(m.group(1))][int(m.group(2))] = (
                    float(m.group(3)), float(m.group(4))
                )
    except FileNotFoundError:
        sys.exit(f"[ERROR] Không tìm thấy file: {filename}")
    if not snapshots:
        sys.exit("[ERROR] Không đọc được vị trí nào.")
    return snapshots

def get_snapshot(data, time_point):
    closest = min(data.keys(), key=lambda t: abs(t - time_point))
    if abs(closest - time_point) > 1.0:
        print(f"  [WARN] Snapshot gần nhất cách {abs(closest-time_point):.2f}s.")
    return data[closest], closest

# ─────────────────────────────────────────────
# FILTER — khớp với isValidForwardNode() trong C++
# ─────────────────────────────────────────────
def is_valid_forward(nx, ny, sx, sy, direction):
    """
    Tái hiện isValidForwardNode() trong BpabMac.cpp.
    direction: 'EAST'|'WEST'|'NORTH'|'SOUTH'
    Trả về (valid: bool, dist_to_src: float)
    """
    dx  = nx - sx
    dy  = ny - sy
    adx = abs(dx)
    ady = abs(dy)
    hw  = WIDTH_W / 2.0   # halfWidth = 15 m

    if direction == 'WEST':
        # dx < -MIN_PROGRESS  (node phía tây source)
        if dx < -MIN_PROGRESS and adx <= RANGE_R and ady <= hw:
            return True, adx
    elif direction == 'EAST':
        if dx > MIN_PROGRESS and dx <= RANGE_R and ady <= hw:
            return True, dx
    elif direction == 'NORTH':
        if dy > MIN_PROGRESS and dy <= RANGE_R and adx <= hw:
            return True, dy
    elif direction == 'SOUTH':
        if dy < -MIN_PROGRESS and ady <= RANGE_R and adx <= hw:
            return True, ady
    return False, 0.0

def infer_direction(src_id, nodes):
    """
    BPAB tính hướng truyền = ngược chiều di chuyển.
    Ở đây dùng heuristic: source có x lớn → xe đang chạy về EAST
    → truyền sang WEST (về phía xe phía trước = x nhỏ hơn).
    Trả về direction string.
    """
    sx, sy = nodes[src_id]
    # Tìm xe có x nhỏ nhất → hướng truyền là WEST
    min_x = min(nodes[n][0] for n in nodes if n != src_id)
    if sx > min_x:
        return 'WEST'
    return 'EAST'

# ─────────────────────────────────────────────
# ĐỒ THỊ — có directional filter
# ─────────────────────────────────────────────
def euclid(a, b):
    return math.hypot(a[0]-b[0], a[1]-b[1])

def build_graph(nodes, src_id, direction):
    """
    Adjacency list với filter giống BPAB:
    - node v hợp lệ nhận từ u nếu isValidForwardNode(v, u, direction) == True
    - Đây là đồ thị có hướng (u → v nếu v hợp lệ theo direction so với u)
    """
    graph = defaultdict(list)   # graph[u] = [(v, dist_from_u_to_v)]
    ids = list(nodes.keys())
    for u in ids:
        ux, uy = nodes[u]
        for v in ids:
            if v == u:
                continue
            vx, vy = nodes[v]
            valid, d = is_valid_forward(vx, vy, ux, uy, direction)
            if valid:
                graph[u].append((v, d))
    return graph

# ─────────────────────────────────────────────
# FLOODING
# ─────────────────────────────────────────────
def flooding(graph, source, nodes):
    received = {source}
    frontier = [source]
    # delay tính bằng giây: mỗi hop = HOP_DURATION_S
    delay  = {source: 0.0}
    parent = {source: None}
    retx   = 0

    while frontier:
        cur = frontier.pop(0)
        for neigh, _ in graph[cur]:
            if neigh in received:
                continue
            received.add(neigh)
            delay[neigh]  = delay[cur] + HOP_DURATION_S
            parent[neigh] = cur
            frontier.append(neigh)
            retx += 1

    dest, d_delay, path = _find_dest(received, delay, parent, source, nodes)
    return {
        "received": len(received),
        "retx"    : retx,
        "dest"    : dest,
        "metrics" : _path_metrics(path, d_delay, nodes) if dest else None,
    }

# ─────────────────────────────────────────────
# WEIGHTED-P
# ─────────────────────────────────────────────
def weighted_p(graph, source, nodes):
    received = {source}
    frontier = [source]
    delay  = {source: 0.0}
    parent = {source: None}
    retx   = 0

    while frontier:
        sender = frontier.pop(0)
        for neigh, d in graph[sender]:
            if neigh in received:
                continue
            p = d / RANGE_R
            if random.random() > p:
                continue
            received.add(neigh)
            delay[neigh]  = delay[sender] + HOP_DURATION_S
            parent[neigh] = sender
            frontier.append(neigh)
            retx += 1

    dest, d_delay, path = _find_dest(received, delay, parent, source, nodes)
    return {
        "received": len(received),
        "retx"    : retx,
        "dest"    : dest,
        "metrics" : _path_metrics(path, d_delay, nodes) if dest else None,
    }

# ─────────────────────────────────────────────
# SLOTTED-P
# ─────────────────────────────────────────────
def slotted_p(graph, source, nodes):
    received = {source}
    frontier = [source]
    delay  = {source: 0.0}
    parent = {source: None}
    retx   = 0

    while frontier:
        sender     = frontier.pop(0)
        candidates = []
        for neigh, d in graph[sender]:
            if neigh in received:
                continue
            slot = int(NUM_SLOTS * (1.0 - d / RANGE_R))
            p    = d / RANGE_R
            candidates.append((slot, neigh, p))
        candidates.sort()

        occupied = False
        for slot, neigh, p in candidates:
            if occupied:
                break
            if random.random() < p:
                received.add(neigh)
                # delay = thời gian RTB + chờ slot + overhead
                # mỗi slot = SLOT_DUR, overhead cố định = (2*MAX_ITER+20)*SLOT_DUR
                slot_delay = SLOT_DUR * slot + HOP_DURATION_S
                delay[neigh]  = delay[sender] + slot_delay
                parent[neigh] = sender
                frontier.append(neigh)
                retx += 1
                occupied = True

    dest, d_delay, path = _find_dest(received, delay, parent, source, nodes)
    return {
        "received": len(received),
        "retx"    : retx,
        "dest"    : dest,
        "metrics" : _path_metrics(path, d_delay, nodes) if dest else None,
    }

# ─────────────────────────────────────────────
# HELPERS
# ─────────────────────────────────────────────
def _find_dest(received, delay, parent, source, nodes):
    candidates = [n for n in received if n != source]
    if not candidates:
        return None, 0, [source]
    dest  = min(candidates, key=lambda n: nodes[n][0])   # x nhỏ nhất
    path  = []
    cur   = dest
    while cur is not None:
        path.append(cur)
        cur = parent.get(cur)
    path.reverse()
    return dest, delay[dest], path

def _path_metrics(path, e2e_delay, nodes):
    n = len(path) - 1
    if n <= 0:
        return None
    total_dist    = sum(euclid(nodes[path[i]], nodes[path[i+1]]) for i in range(n))
    avg_hop_delay = e2e_delay / n
    msg_progress  = total_dist / n
    speed         = msg_progress / avg_hop_delay if avg_hop_delay > 0 else 0.0
    return {
        "dest"           : path[-1],
        "num_hops"       : n,
        "total_dist_m"   : total_dist,
        "e2e_delay_s"    : e2e_delay,
        "avg_hop_delay_s": avg_hop_delay,
        "msg_progress_m" : msg_progress,
        "speed_m_s"      : speed,
    }

def monte_carlo(fn, graph, source, nodes, runs):
    mk = ["e2e_delay_s","avg_hop_delay_s","msg_progress_m","speed_m_s","num_hops"]
    acc_rcv = acc_retx = 0.0
    acc_m   = defaultdict(float)
    valid   = 0
    dest_v  = defaultdict(int)

    for _ in range(runs):
        r = fn(graph, source, nodes)
        acc_rcv  += r["received"]
        acc_retx += r["retx"]
        if r["metrics"]:
            valid += 1
            for k in mk:
                acc_m[k] += r["metrics"][k]
            dest_v[r["dest"]] += 1

    avg_dest = max(dest_v, key=dest_v.get) if dest_v else None
    avg_m    = None
    if valid:
        avg_m = {k: acc_m[k]/valid for k in mk}
        avg_m["dest"] = avg_dest

    return {
        "received": acc_rcv / runs,
        "retx"    : acc_retx / runs,
        "metrics" : avg_m,
    }

# ─────────────────────────────────────────────
# IN BẢNG
# ─────────────────────────────────────────────
def print_comparison_table(algo_results, nodes):
    W = 92
    print(f"\n{'═'*W}")
    print(f"{'BẢNG SO SÁNH HIỆU NĂNG':^{W}}")
    print(f"{'(delay tính bằng giây thực — cùng đơn vị với BPAB analyzer)':^{W}}")
    print(f"{'═'*W}")

    c0 = 16
    cols = [
        ("E2E Delay (s)",    14),
        ("Avg Hop Delay (s)",18),
        ("Progress/Hop (m)", 17),
        ("Speed (m/s)",      13),
        ("Hops",              6),
        ("Node đích",        10),
    ]
    hdr = f"  {'Algorithm':<{c0}}"
    for lbl, w in cols:
        hdr += f"| {lbl:^{w}}"
    print(hdr)
    print(f"  {'─'*c0}" + "".join(f"+{'─'*w}──" for _, w in cols))

    for algo, res in algo_results.items():
        m = res.get("metrics")
        dest_id = m["dest"] if m else None
        dest_x  = f"{nodes[dest_id][0]:.0f}m" if dest_id and dest_id in nodes else "?"

        row = f"  {algo:<{c0}}"
        if m:
            vals = [
                f"{m['e2e_delay_s']:.4f}",
                f"{m['avg_hop_delay_s']:.4f}",
                f"{m['msg_progress_m']:.2f}",
                f"{m['speed_m_s']:.2f}",
                str(int(round(m['num_hops']))),
                f"N{dest_id}(x={dest_x})",
            ]
        else:
            vals = ["N/A"] * 6
        for val, (_, w) in zip(vals, cols):
            row += f"| {val:^{w}}"
        print(row)
    print(f"{'═'*W}\n")

def print_coverage_table(algo_results, total_nodes):
    print(f"  {'─'*52}")
    print(f"  {'Algorithm':<18} | {'Nodes rcv':>9} | {'Coverage':>9} | {'Retx':>6}")
    print(f"  {'─'*52}")
    for algo, res in algo_results.items():
        r = res["received"]
        print(f"  {algo:<18} | {r:>9.1f} | {r/total_nodes*100:>8.1f}% | {res['retx']:>6.1f}")
    print(f"  {'─'*52}\n")

def print_config_summary():
    print(f"\n  Tham số calibrated từ omnetpp.ini + BpabMac.cpp defaults:")
    print(f"    rangeR        = {RANGE_R} m")
    print(f"    widthW        = {WIDTH_W} m  (halfWidth = {WIDTH_W/2} m)")
    print(f"    minProgress   = {MIN_PROGRESS} m")
    print(f"    slotDuration  = {SLOT_DUR} s")
    print(f"    maxIterations = {MAX_ITER}")
    print(f"    HOP_DURATION  = {HOP_DURATION_S*1000:.2f} ms  (= slotDur × (2×maxIter+20))")

# ─────────────────────────────────────────────
# MAIN
# ─────────────────────────────────────────────
def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--log",    default=DEFAULT_LOG)
    p.add_argument("--time",   default=DEFAULT_TIME,   type=float)
    p.add_argument("--source", default=DEFAULT_SOURCE, type=int)
    p.add_argument("--runs",   default=MC_RUNS,        type=int)
    return p.parse_args()

def main():
    args = parse_args()

    print(f"\n{'═'*60}")
    print("  VANET Broadcast Comparison — BPAB-calibrated")
    print(f"{'═'*60}")
    print(f"  Log      : {args.log}")
    print(f"  Snapshot : {args.time} s")
    print(f"  Source   : Node {args.source}")
    print(f"  MC runs  : {args.runs}")
    print_config_summary()

    data = load_positions(args.log)
    nodes, actual_t = get_snapshot(data, args.time)
    print(f"\n  Snapshot dùng: t={actual_t} s  ({len(nodes)} nodes)")

    if args.source not in nodes:
        sys.exit(f"[ERROR] Node {args.source} không có trong snapshot.")

    direction = infer_direction(args.source, nodes)
    sx, sy    = nodes[args.source]
    dest_t    = min((n for n in nodes if n != args.source), key=lambda n: nodes[n][0])
    dx, dy    = nodes[dest_t]
    print(f"  Node nguồn : {args.source}  (x={sx:.1f} m)")
    print(f"  Hướng truyền: {direction}")
    print(f"  Node đích  : {dest_t}  (x={dx:.1f} m)  ← x nhỏ nhất")

    graph       = build_graph(nodes, args.source, direction)
    total_nodes = len(nodes)
    print(f"  Node {args.source} có {len(graph[args.source])} neighbor(s) hợp lệ theo hướng {direction}.\n")

    r_flood = flooding(graph, args.source, nodes)
    r_wp    = monte_carlo(weighted_p, graph, args.source, nodes, args.runs)
    r_sp    = monte_carlo(slotted_p,  graph, args.source, nodes, args.runs)

    algo_results = {
        "Flooding"  : r_flood,
        "Weighted-P": r_wp,
        "Slotted-P" : r_sp,
    }

    print_comparison_table(algo_results, nodes)
    print_coverage_table(algo_results, total_nodes)

if __name__ == "__main__":
    main()