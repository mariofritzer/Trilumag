"""Kleiner Raster-Router (2 Lagen, A*) für die letzten offenen Verbindungen nach Freerouting."""
import heapq, math
import numpy as np
import pcbnew
from PIL import Image, ImageDraw

MM = pcbnew.ToMM
U = pcbnew.FromMM
G = 0.2            # Rastergröße mm


class Grid:
    def __init__(self, b, x0, y0, x1, y1):
        self.b, self.x0, self.y0 = b, x0, y0
        self.w, self.h = int((x1 - x0) / G) + 1, int((y1 - y0) / G) + 1

    def px(self, x, y):
        return ((x - self.x0) / G, (y - self.y0) / G)

    def cell(self, x, y):
        return int(round((x - self.x0) / G)), int(round((y - self.y0) / G))

    def mm(self, i, j):
        return (float(self.x0 + int(i) * G), float(self.y0 + int(j) * G))

    def raster(self, items, infl):
        im = Image.new('1', (self.w, self.h), 0)
        d = ImageDraw.Draw(im)
        r = infl / G
        for kind, geo, extra in items:
            if kind == 'poly':
                pts = [self.px(*p) for p in geo]
                d.polygon(pts, fill=1)
                if r > 0:
                    d.line(pts + [pts[0]], fill=1, width=int(2 * r) + 1)
                    for p in pts:
                        d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=1)
            elif kind == 'seg':
                (a, b2), wid = geo, extra
                rr = wid / 2 / G + r
                pa, pb = self.px(*a), self.px(*b2)
                d.line([pa, pb], fill=1, width=int(2 * rr) + 1)
                for p in (pa, pb):
                    d.ellipse([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], fill=1)
            elif kind == 'circ':
                p, rad = self.px(*geo), extra / G + r
                d.ellipse([p[0] - rad, p[1] - rad, p[0] + rad, p[1] + rad], fill=1)
        return np.array(im, dtype=bool).T      # [i, j]


def pad_poly(pd):
    sp = pd.GetEffectivePolygon()
    o = sp.Outline(0)
    return [(MM(o.CPoint(i).x), MM(o.CPoint(i).y)) for i in range(o.PointCount())]


def collect(b, netname, layer):
    """Hindernisse (fremde Netze) und Ziele (eigenes Netz) auf einer Lage."""
    obst, own = [], []
    for f in b.GetFootprints():
        for pd in f.Pads():
            if not pd.IsOnLayer(layer):
                continue
            item = ('poly', pad_poly(pd), None)
            (own if pd.GetNetname() == netname else obst).append(item)
    for t in b.GetTracks():
        same = t.GetNetname() == netname
        if t.GetClass() == 'PCB_VIA':
            item = ('circ', (MM(t.GetPosition().x), MM(t.GetPosition().y)), MM(t.GetWidth()) / 2)
        else:
            if t.GetLayer() != layer:
                continue
            a, c = t.GetStart(), t.GetEnd()
            item = ('seg', ((MM(a.x), MM(a.y)), (MM(c.x), MM(c.y))), MM(t.GetWidth()))
        (own if same else obst).append(item)
    for z in b.Zones():
        if z.GetIsRuleArea() and z.GetDoNotAllowTracks() and z.IsOnLayer(layer):
            ol = z.Outline().Outline(0)
            obst.append(('poly', [(MM(ol.CPoint(i).x), MM(ol.CPoint(i).y)) for i in range(ol.PointCount())], None))
    return obst, own


def route(b, netname, src_pad, edge_poly, width=0.3, clear=0.25, via=(0.8, 0.4), margin=25):
    """Verbindet src_pad (PAD) mit dem übrigen Kupfer desselben Netzes."""
    sx, sy = MM(src_pad.GetPosition().x), MM(src_pad.GetPosition().y)
    xs = [p[0] for p in edge_poly]; ys = [p[1] for p in edge_poly]
    g = Grid(b, min(xs), min(ys), max(xs), max(ys))
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
    blocked, target, vblock = [], [], None
    srcpoly = ('poly', pad_poly(src_pad), None)
    for L in layers:
        obst, own = collect(b, netname, L)
        # Platinenrand
        outside = Image.new('1', (g.w, g.h), 1)
        ImageDraw.Draw(outside).polygon([g.px(*p) for p in edge_poly], fill=0)
        ob = g.raster(obst, clear + width / 2) | np.array(outside, dtype=bool).T
        er = g.raster([('poly', edge_poly, None)], 0)
        ob |= ~g.raster_inset(edge_poly, 0.6 + width / 2) if hasattr(g, 'raster_inset') else False
        own_wo_src = [o for o in own if not (o[0] == 'poly' and o[1] == srcpoly[1])]
        tg = g.raster(own_wo_src, 0)
        blocked.append(ob)
        target.append(tg & ~ob | tg)
        vb = g.raster(obst, clear + via[0] / 2)
        vblock = vb if vblock is None else (vblock | vb)
    # Rand-Abstand
    inner = Image.new('1', (g.w, g.h), 0)
    from shapely.geometry import Polygon
    ip = Polygon(edge_poly).buffer(-(0.6 + width / 2))
    ImageDraw.Draw(inner).polygon([g.px(*p) for p in ip.exterior.coords], fill=1)
    inner = np.array(inner, dtype=bool).T
    for k in range(2):
        blocked[k] |= ~inner
    vblock |= ~inner
    # Start: Zellen des Quellpads, die frei sind
    starts = []
    sp = g.raster([srcpoly], 0)
    for k, L in enumerate(layers):
        if not src_pad.IsOnLayer(L):
            continue
        ii, jj = np.nonzero(sp)
        for i, j in zip(ii, jj):
            starts.append((i, j, k))
            blocked[k][i, j] = False
    # Quellpad selbst nicht als Ziel
    for k in range(2):
        target[k] &= ~sp
    tx, ty = None, None
    nb = [(1, 0, 1), (-1, 0, 1), (0, 1, 1), (0, -1, 1), (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
    tgt_idx = [np.argwhere(target[k]) for k in range(2)]
    allt = np.vstack([t for t in tgt_idx if len(t)]) if any(len(t) for t in tgt_idx) else None
    if allt is None:
        return False

    def hfun(i, j):
        d = np.abs(allt[::max(1, len(allt) // 400)] - [i, j]).max(axis=1)
        return float(d.min())
    openq, came, cost = [], {}, {}
    for s in starts:
        cost[s] = 0
        heapq.heappush(openq, (0, s))
    VIA_COST = 12
    end = None
    while openq:
        _, cur = heapq.heappop(openq)
        i, j, k = cur
        if target[k][i, j]:
            end = cur
            break
        c0 = cost[cur]
        for di, dj, dc in nb:
            n = (i + di, j + dj, k)
            if not (0 <= n[0] < g.w and 0 <= n[1] < g.h) or blocked[k][n[0], n[1]]:
                continue
            nc = c0 + dc + (0.3 if (cur in came and (came[cur][0] - i, came[cur][1] - j) != (-di, -dj)) else 0)
            if nc < cost.get(n, 1e18):
                cost[n] = nc; came[n] = cur
                heapq.heappush(openq, (nc + hfun(*n[:2]) * 0.9, n))
        if not vblock[i, j]:
            n = (i, j, 1 - k)
            if not blocked[1 - k][i, j]:
                nc = c0 + VIA_COST
                if nc < cost.get(n, 1e18):
                    cost[n] = nc; came[n] = cur
                    heapq.heappush(openq, (nc + hfun(i, j) * 0.9, n))
    if end is None:
        return False
    path = [end]
    while path[-1] in came:
        path.append(came[path[-1]])
    path.reverse()
    net = b.FindNet(netname)
    # in Segmente (gleiche Richtung zusammenfassen) und Vias umsetzen
    segs = []
    start = path[0]
    prev = path[0]
    pdir = None
    for p in path[1:]:
        if p[2] != prev[2]:
            if prev != start:
                segs.append((start, prev))
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(pcbnew.VECTOR2I(U(g.mm(prev[0], prev[1])[0]), U(g.mm(prev[0], prev[1])[1])))
            v.SetWidth(U(via[0])); v.SetDrill(U(via[1])); v.SetNet(net)
            v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            b.Add(v)
            start, prev, pdir = p, p, None
            continue
        d = (p[0] - prev[0], p[1] - prev[1])
        if pdir is not None and d != pdir:
            segs.append((start, prev))
            start = prev
        pdir = d
        prev = p
    if prev != start:
        segs.append((start, prev))
    # erstes Stück im Padmittelpunkt beginnen
    for n_, (a, c) in enumerate(segs):
        t = pcbnew.PCB_TRACK(b)
        pa = g.mm(a[0], a[1]) if n_ else (sx, sy) if a[2] == path[0][2] else g.mm(a[0], a[1])
        pc = g.mm(c[0], c[1])
        t.SetStart(pcbnew.VECTOR2I(U(pa[0]), U(pa[1]))); t.SetEnd(pcbnew.VECTOR2I(U(pc[0]), U(pc[1])))
        t.SetWidth(U(width)); t.SetLayer(layers[a[2]]); t.SetNet(net)
        b.Add(t)
    return True
