"""Offene Verbindungen nach dem Autorouter mit dem Raster-Router schließen, Zonen neu füllen."""
import sys, re, pcbnew
import router
from board import outline
path = sys.argv[1]
b = pcbnew.LoadBoard(path)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.WriteDRCReport(b, '/tmp/f.txt', pcbnew.EDA_UNITS_MILLIMETRES, True)
t = open('/tmp/f.txt').read()
todo = []
for bl in re.split(r'\n(?=\[)', t):
    if not bl.startswith('[unconnected_items]'): continue
    m = re.findall(r'(?:PTH )?[Pp]ad (\S+) \[([^\]]+)\] of (\S+)', bl)
    if m:
        todo.append(m[0])
print('offen:', todo)
fps = {f.GetReference(): f for f in b.GetFootprints()}
for num, net, ref in todo:
    pd = [p for p in fps[ref].Pads() if p.GetNumber() == num][0]
    for w, c in ((0.3, 0.25), (0.25, 0.2)):
        ok = router.route(b, net, pd, outline(), width=w, clear=c)
        if ok: break
    print(ref, num, net, 'verbunden' if ok else 'NICHT verbunden')
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.BuildConnectivity()
print('offen danach:', b.GetConnectivity().GetUnconnectedCount(True))
pcbnew.SaveBoard(path, b)
t = open(path).read().replace('(paper "A4")', '(paper "A3")', 1); open(path, 'w').write(t)
