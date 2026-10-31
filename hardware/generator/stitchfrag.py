import sys, pcbnew
from shapely.geometry import Polygon, Point
P = sys.argv[1]
b = pcbnew.LoadBoard(P)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
gnd = b.FindNet('GND')
M = lambda v: v / 1e6
fr = {pcbnew.F_Cu: [], pcbnew.B_Cu: []}
for z in b.Zones():
    if z.GetIsRuleArea(): continue
    for L in fr:
        if not z.IsOnLayer(L): continue
        sp = z.GetFilledPolysList(L)
        for i in range(sp.OutlineCount()):
            o = sp.Outline(i)
            fr[L].append(Polygon([(M(o.CPoint(k).x), M(o.CPoint(k).y)) for k in range(o.PointCount())]).buffer(0))
n = 0
for L, other in ((pcbnew.F_Cu, pcbnew.B_Cu), (pcbnew.B_Cu, pcbnew.F_Cu)):
    main_o = max(fr[other], key=lambda p: p.area)
    main_s = max(fr[L], key=lambda p: p.area)
    for pg in fr[L]:
        if pg is main_s or pg.area < 2: continue
        ok = pg.buffer(-0.55).intersection(main_o.buffer(-0.55))
        if ok.is_empty: print('kein Platz', pcbnew.LayerName(L), round(pg.area, 1)); continue
        p = ok.representative_point()
        v = pcbnew.PCB_VIA(b); v.SetPosition(pcbnew.VECTOR2I(int(p.x * 1e6), int(p.y * 1e6))); v.SetWidth(800000); v.SetDrill(400000)
        v.SetNet(gnd); v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); b.Add(v); n += 1
print('Vias', n)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
