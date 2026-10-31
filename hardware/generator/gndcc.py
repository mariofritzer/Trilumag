import sys, pcbnew
from shapely.geometry import Polygon, Point, LineString
b = pcbnew.LoadBoard(sys.argv[1]); pcbnew.ZONE_FILLER(b).Fill(b.Zones())
M = lambda v: v / 1e6
F, B = pcbnew.F_Cu, pcbnew.B_Cu
items = []  # (name, layers, geom)
for z in b.Zones():
    if z.GetIsRuleArea(): continue
    for L in (F, B):
        if not z.IsOnLayer(L): continue
        sp = z.GetFilledPolysList(L)
        for i in range(sp.OutlineCount()):
            o = sp.Outline(i); pg = Polygon([(M(o.CPoint(k).x), M(o.CPoint(k).y)) for k in range(o.PointCount())]).buffer(0)
            items.append((f'zone{pcbnew.LayerName(L)}#{i} A={pg.area:.1f} {[round(v,1) for v in pg.bounds]}', {L}, pg))
for f in b.GetFootprints():
    for p in f.Pads():
        if p.GetNetname() != 'GND': continue
        sp = p.GetEffectivePolygon(); o = sp.Outline(0)
        items.append((f'{f.GetReference()}.{p.GetNumber()}', {L for L in (F, B) if p.IsOnLayer(L)}, Polygon([(M(o.CPoint(k).x), M(o.CPoint(k).y)) for k in range(o.PointCount())]).buffer(0)))
for t in b.GetTracks():
    if t.GetNetname() != 'GND': continue
    if t.GetClass() == 'PCB_VIA':
        items.append(('via', {F, B}, Point(M(t.GetPosition().x), M(t.GetPosition().y)).buffer(M(t.GetWidth()) / 2)))
    else:
        items.append(('trk', {t.GetLayer()}, LineString([(M(t.GetStart().x), M(t.GetStart().y)), (M(t.GetEnd().x), M(t.GetEnd().y))]).buffer(M(t.GetWidth()) / 2)))
par = list(range(len(items)))
def fd(i):
    while par[i] != i: par[i] = par[par[i]]; i = par[i]
    return i
for i in range(len(items)):
    for j in range(i + 1, len(items)):
        if items[i][1] & items[j][1] and items[i][2].intersects(items[j][2]):
            par[fd(i)] = fd(j)
comp = {}
for i in range(len(items)): comp.setdefault(fd(i), []).append(items[i][0])
for c in sorted(comp.values(), key=len):
    print(len(c), c[:8])
