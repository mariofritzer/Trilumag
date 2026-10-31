"""Offene Pads mit feinem Raster an die Hauptinsel ihres Netzes anbinden."""
import sys, re, pcbnew, math
import router
from board import outline
path = sys.argv[1]
grid = float(sys.argv[2]) if len(sys.argv) > 2 else 0.1
router.G = grid
b = pcbnew.LoadBoard(path)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.WriteDRCReport(b, '/tmp/r.txt', pcbnew.EDA_UNITS_MILLIMETRES, True)
t = open('/tmp/r.txt').read()
todo = []
for bl in re.split(r'\n(?=\[)', t):
    if bl.startswith('[unconnected_items]'):
        for m in re.findall(r'(?:PTH )?[Pp]ad (\S+) \[([^\]]+)\] of (\S+)', bl):
            if m not in todo: todo.append(m)
import os
prio = open('/tmp/prio.txt').read().split(',') if os.path.exists('/tmp/prio.txt') else ['TINT']
todo.sort(key=lambda m: m[1] not in prio)
b = pcbnew.LoadBoard(path)
fps = {f.GetReference(): f for f in b.GetFootprints()}
for num, net, ref in todo:
    pd = [p for p in fps[ref].Pads() if p.GetNumber() == num][0]
    # Hauptinsel = größte Insel des Netzes, die das Pad nicht enthält
    allpads = [p for f in b.GetFootprints() for p in f.Pads() if p.GetNetname() == net]
    best = None
    for sp in allpads:
        tg, mem = router.island(b, net, sp)
        if any(m.GetClass() == 'PAD' and m.GetParent().GetReference() == ref and m.GetNumber() == num for m in mem):
            continue
        if best is None or len(mem) > len(best[1]): best = (tg, mem)
    if best is None:
        print(ref, num, net, 'schon verbunden'); continue
    ok = False
    for w, c in ((0.3, 0.3), (0.3, 0.2), (0.25, 0.2)):
        ok = router.route(b, net, pd, outline(), width=w, clear=c, targets=best[0])
        if ok: break
    print(ref, num, net, 'verbunden' if ok else 'NICHT verbunden')
pcbnew.SaveBoard(path, b)
b = pcbnew.LoadBoard(path)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.BuildConnectivity()
print('offen danach:', b.GetConnectivity().GetUnconnectedCount(True))
pcbnew.SaveBoard(path, b)
s = open(path).read().replace('(paper "A4")', '(paper "A3")', 1); open(path, 'w').write(s)
