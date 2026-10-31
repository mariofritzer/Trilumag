import sys, re, math, pcbnew, subprocess
path = sys.argv[1]
def unconnected():
    b = pcbnew.LoadBoard(path); pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    pcbnew.WriteDRCReport(b, '/tmp/u.txt', pcbnew.EDA_UNITS_MILLIMETRES, True)
    t = open('/tmp/u.txt').read(); out = []
    for bl in re.split(r'\n(?=\[)', t):
        if bl.startswith('[unconnected_items]'):
            out += re.findall(r'(?:PTH )?[Pp]ad (\S+) \[([^\]]+)\] of (\S+)', bl)
    return out
for rnd in [int(sys.argv[2]) if len(sys.argv) > 2 else 0]:
    todo = unconnected()
    print('Runde', rnd, 'offen', len(todo))
    if not todo: break
    b = pcbnew.LoadBoard(path)
    fps = {f.GetReference(): f for f in b.GetFootprints()}
    pts = []
    for num, net, ref in todo:
        p = [x for x in fps[ref].Pads() if x.GetNumber() == num][0].GetPosition()
        pts.append((pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)))
    rad = 2.5 + rnd
    rm = set()
    for t in b.GetTracks():
        if t.IsLocked() or t.GetNetname() == 'GND': continue
        if t.GetClass() == 'PCB_VIA':
            q = t.GetPosition(); segs = [((pcbnew.ToMM(q.x), pcbnew.ToMM(q.y)),) * 2]
        else:
            a, e = t.GetStart(), t.GetEnd(); segs = [((pcbnew.ToMM(a.x), pcbnew.ToMM(a.y)), (pcbnew.ToMM(e.x), pcbnew.ToMM(e.y)))]
        (ax, ay), (ex, ey) = segs[0]
        for px, py in pts:
            L = (ex - ax) ** 2 + (ey - ay) ** 2 or 1e-9
            k = max(0, min(1, ((px - ax) * (ex - ax) + (py - ay) * (ey - ay)) / L))
            if math.hypot(ax + k * (ex - ax) - px, ay + k * (ey - ay) - py) < rad:
                rm.add(t.GetNetname())
    first = {n for _, n, _ in todo}
    print('reiße auf', rm)
    for t in list(b.GetTracks()):
        if t.GetNetname() in rm and not t.IsLocked(): b.Remove(t)
    pcbnew.SaveBoard(path, b)
    open('/tmp/prio.txt', 'w').write(','.join(first))
    print(subprocess.run(['python3', 'repair.py', path, '0.05'], capture_output=True, text=True).stdout.strip().splitlines()[-1])
