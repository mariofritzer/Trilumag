import sys, pcbnew, math
P = sys.argv[1]
b = pcbnew.LoadBoard(P)
U = lambda v: int(round(v * 1e6)); V = lambda x, y: pcbnew.VECTOR2I(U(x), U(y))
N = {n: b.FindNet(n) for n in ('VIN', 'VIN_F', 'GND')}
tr = list(b.GetTracks())
for t in tr:
    if t.GetClass() == 'PCB_VIA' and t.GetNetname() == 'VBUS' and t.IsLocked():
        t.SetWidth(U(0.6)); t.SetDrill(U(0.3))
# TX-Via vom DE-Track wegschieben
de = [t for t in tr if t.GetNetname() == 'DE' and t.GetClass() == 'PCB_TRACK' and abs(t.GetLength() / 1e6 - 16.175) < 0.01][0]
for t in tr:
    if t.GetClass() == 'PCB_VIA' and t.GetNetname() == 'TX' and abs(t.GetPosition().x / 1e6 - 204.8) < 0.01 and abs(t.GetPosition().y / 1e6 - 144.07) < 0.01:
        a, c, p = de.GetStart(), de.GetEnd(), t.GetPosition()
        ax, ay, cx, cy, px, py = a.x / 1e6, a.y / 1e6, c.x / 1e6, c.y / 1e6, p.x / 1e6, p.y / 1e6
        dx, dy = cx - ax, cy - ay; L = dx * dx + dy * dy
        s = max(0, min(1, ((px - ax) * dx + (py - ay) * dy) / L)); qx, qy = ax + s * dx, ay + s * dy
        d = math.hypot(px - qx, py - qy); ux, uy = (px - qx) / d, (py - qy) / d
        t.SetPosition(V(px + ux * 0.06, py + uy * 0.06)); print('TX-Via verschoben', round(d, 3))
for x, n in ((171.0, 'VIN'),):
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        t = pcbnew.PCB_TRACK(b); t.SetStart(V(x, 167)); t.SetEnd(V(x, 172)); t.SetWidth(U(2.0)); t.SetLayer(L); t.SetNet(N[n]); b.Add(t)
# X3B: Pin 5 und Pin 1 (beide GND) außen auf B.Cu verbinden
pts = [(150.0, 158.75), (147.3, 158.75), (147.3, 172.75), (150.0, 172.75)]
for a, c in []:
    t = pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(U(0.5)); t.SetLayer(pcbnew.B_Cu); t.SetNet(N['GND']); t.SetLocked(True); b.Add(t)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
