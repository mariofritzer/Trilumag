import sys, pcbnew
P = sys.argv[1]
b = pcbnew.LoadBoard(P)
U = lambda v: int(round(v * 1e6)); V = lambda x, y: pcbnew.VECTOR2I(U(x), U(y))
fps = {f.GetReference(): f for f in b.GetFootprints()}
nets = {n: b.FindNet(n) for n in ('VIN', 'VIN_F')}
# 1) überbreite Bibliotheks-Antennensperrfläche des Moduls entfernen (eigene Sperrfläche 198..222 bleibt)
u3 = fps['U3']
for z in list(u3.Zones()):
    u3.Remove(z)
# 2) Sicherungshalter: beide Pins je Clip verbinden
for x, n in ((178.0, 'VIN'), (195.8, 'VIN_F')):
    for L in (pcbnew.F_Cu, pcbnew.B_Cu):
        t = pcbnew.PCB_TRACK(b); t.SetStart(V(x, 207)); t.SetEnd(V(x, 212)); t.SetWidth(U(2.0)); t.SetLayer(L); t.SetNet(nets[n]); b.Add(t)
# 3) dünne Endstücke an Leistungsnetzen verbreitern
for t in b.GetTracks():
    if t.GetClass() == 'PCB_TRACK' and t.GetNetname() in ('VIN', 'VIN_F', '+24V_ROH', '+24V') and t.GetWidth() < U(1.0):
        t.SetWidth(U(1.0))
# 4) Masseinseln entfernen
for z in b.Zones():
    if not z.GetIsRuleArea():
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
print('ok')
