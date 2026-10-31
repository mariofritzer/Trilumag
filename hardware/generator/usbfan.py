"""Hand-Fanout für USB-C J4 (0,5-mm-Raster ist für den Rasterrouter zu eng)."""
import sys, pcbnew
P = sys.argv[1]
b = pcbnew.LoadBoard(P)
fp = {f.GetReference(): f for f in b.GetFootprints()}
NETS = {n: b.FindNet(n) for n in ['USB_DN','USB_DP','CC1','CC2','VBUS']}
U = lambda v: int(round(v * 1e6))
V = lambda x, y: pcbnew.VECTOR2I(U(x), U(y))
# R5/R6 und deren Masse-Fanout weg vom Stecker
for t in list(b.GetTracks()):
    p = t.GetPosition()
    if t.GetNetname() == 'GND' and 276e6 < p.x < 292e6 and 218e6 < p.y < 229e6:
        b.Remove(t)
fp['R5'].SetPosition(V(276.5, 224.0))
fp['R6'].SetPosition(V(290.5, 225.5))
def trk(net, pts, w=0.2, layer=pcbnew.F_Cu):
    n = NETS[net]
    for a, c in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(U(w)); t.SetLayer(layer); t.SetNet(n); t.SetLocked(True); b.Add(t)
def via(net, x, y, d=0.6, dr=0.3):
    v = pcbnew.PCB_VIA(b); v.SetPosition(V(x, y)); v.SetWidth(U(d)); v.SetDrill(U(dr)); v.SetNet(NETS[net])
    v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); v.SetLocked(True); b.Add(v)
Y = 227.795
# D- : B7 und A7 unter dem Pad-Reihe verbinden, Ausgang nach oben über B7
trk('USB_DN', [(283.25, Y), (283.25, 228.8), (284.25, 228.8), (284.25, Y)])
trk('USB_DN', [(283.25, Y), (283.25, 225.0), (283.25, 223.0)])
# D+ : A6 und B6 oberhalb verbinden, Ausgang über die Mitte
trk('USB_DP', [(283.75, Y), (283.75, 226.3), (284.75, 226.3), (284.75, Y)])
trk('USB_DP', [(284.25, 226.3), (284.25, 225.0), (284.75, 224.5), (284.75, 223.0)])
# CC1 / CC2 nach außen auffächern
trk('CC1', [(282.75, Y), (282.75, 225.0), (281.25, 223.5)])
trk('CC2', [(285.75, Y), (285.75, 225.0), (287.25, 223.5)])
# VBUS: beide Seiten auf Vias, auf B.Cu verbinden
for x in (281.6, 286.4):
    trk('VBUS', [(x, Y), (x, 226.4)], w=0.4)
    via('VBUS', x, 226.4)
trk('VBUS', [(281.6, 226.4), (286.4, 226.4)], w=0.4, layer=pcbnew.B_Cu)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
print('ok')
