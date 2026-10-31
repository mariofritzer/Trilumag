"""Schmale Leistungsbahnen so breit wie möglich machen (max. 2 mm), ohne Abstand zu fremden Netzen zu verletzen."""
import sys, pcbnew
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import unary_union
from board import outline
P = sys.argv[1]; NETS = sys.argv[2].split(',')
b = pcbnew.LoadBoard(P)
M = lambda v: v / 1e6
def items(layer):
    out = []
    for f in b.GetFootprints():
        for p in f.Pads():
            if p.IsOnLayer(layer):
                o = p.GetEffectivePolygon().Outline(0)
                out.append((p.GetNetname(), Polygon([(M(o.CPoint(i).x), M(o.CPoint(i).y)) for i in range(o.PointCount())])))
    for t in b.GetTracks():
        if t.GetClass() == 'PCB_VIA':
            out.append((t.GetNetname(), Point(M(t.GetPosition().x), M(t.GetPosition().y)).buffer(M(t.GetWidth()) / 2)))
        elif t.GetLayer() == layer:
            out.append((t.GetNetname(), LineString([(M(t.GetStart().x), M(t.GetStart().y)), (M(t.GetEnd().x), M(t.GetEnd().y))]).buffer(M(t.GetWidth()) / 2), t))
    return out
edge = Polygon(outline()).buffer(-0.6)
n = 0
for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
    its = items(layer)
    for t in list(b.GetTracks()):
        if t.GetClass() != 'PCB_TRACK' or t.GetLayer() != layer or t.GetNetname() not in NETS or t.GetWidth() >= 1999000:
            continue
        ln = LineString([(M(t.GetStart().x), M(t.GetStart().y)), (M(t.GetEnd().x), M(t.GetEnd().y))])
        if ln.length < 1e-6: continue
        others = unary_union([g for it in its for g in [it[1]] if it[0] != t.GetNetname()])
        for w in (2.0, 1.6, 1.3, 1.0, 0.8, 0.6, 0.5, 0.4):
            if w * 1e6 <= t.GetWidth(): break
            g = ln.buffer(w / 2)
            if edge.contains(g) and g.distance(others) >= 0.21:
                t.SetWidth(int(w * 1e6)); n += 1; break
print('verbreitert', n)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
