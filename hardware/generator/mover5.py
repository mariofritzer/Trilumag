import pcbnew, math, sys
from shapely.geometry import Polygon, LineString, Point, box
from shapely.ops import unary_union
p = sys.argv[1]; ref = sys.argv[2]; netn = sys.argv[3]
b = pcbnew.LoadBoard(p)
MM = pcbnew.ToMM; U = pcbnew.FromMM
f = [x for x in b.GetFootprints() if x.GetReference() == ref][0]
# Hindernisse: Kupfer anderer Netze auf F.Cu
obs = []
for g in b.GetFootprints():
    if g is f: continue
    for pd in g.Pads():
        if pd.GetNetname() == netn or not pd.IsOnLayer(pcbnew.F_Cu): continue
        bb = pd.GetBoundingBox(); obs.append(box(MM(bb.GetX()), MM(bb.GetY()), MM(bb.GetRight()), MM(bb.GetBottom())))
    bb = g.GetBoundingBox(False, False)
pts = []
for t in b.GetTracks():
    if t.GetClass() == 'PCB_VIA':
        c = Point(MM(t.GetPosition().x), MM(t.GetPosition().y)).buffer(MM(t.GetWidth()) / 2)
        if t.GetNetname() != netn: obs.append(c)
        continue
    if t.GetLayer() != pcbnew.F_Cu: continue
    a, e = t.GetStart(), t.GetEnd()
    ln = LineString([(MM(a.x), MM(a.y)), (MM(e.x), MM(e.y))]) if a != e else Point(MM(a.x), MM(a.y))
    if t.GetNetname() == netn:
        L = ln.length if hasattr(ln, 'length') else 0
        for k in range(0, int(L / 0.25) + 1):
            q = ln.interpolate(k * 0.25); pts.append((q.x, q.y))
    else:
        obs.append(ln.buffer(MM(t.GetWidth()) / 2))
occ = unary_union(obs).buffer(0.3)
# Pad-Geometrie relativ zu Pad 1
p1 = [x for x in f.Pads() if x.GetNumber() == '1'][0]
f.SetOrientationDegrees(0); f.SetPosition(pcbnew.VECTOR2I(0, 0))
off = (MM(p1.GetPosition().x), MM(p1.GetPosition().y))
fb = f.GetBoundingBox(False, False)
rect = [(MM(fb.GetX()), MM(fb.GetY())), (MM(fb.GetRight()), MM(fb.GetY())), (MM(fb.GetRight()), MM(fb.GetBottom())), (MM(fb.GetX()), MM(fb.GetBottom()))]
fps = [box(*g.GetBoundingBox(False, False).GetX().__class__ and (MM(g.GetBoundingBox(False,False).GetX()), MM(g.GetBoundingBox(False,False).GetY()), MM(g.GetBoundingBox(False,False).GetRight()), MM(g.GetBoundingBox(False,False).GetBottom()))) for g in b.GetFootprints() if g is not f and g.GetReference()[0] not in 'HL']
fpocc = unary_union(fps).buffer(0.2)
best = None
for (x, y) in pts:
    for rot in (0, 90, 180, 270):
        a = math.radians(rot); c, s = math.cos(a), math.sin(a)
        ox, oy = off[0] * c + off[1] * s, -off[0] * s + off[1] * c
        pos = (x - ox, y - oy)
        poly = Polygon([(pos[0] + u * c + v * s, pos[1] - u * s + v * c) for u, v in rect])
        if poly.intersects(occ) or poly.intersects(fpocc): continue
        best = (pos, rot); break
    if best: break
print('Platz:', best)
if best:
    f.SetPosition(pcbnew.VECTOR2I(U(best[0][0]), U(best[0][1]))); f.SetOrientationDegrees(best[1])
    for t in list(b.GetTracks()):
        pass
    pcbnew.SaveBoard(p, b)
