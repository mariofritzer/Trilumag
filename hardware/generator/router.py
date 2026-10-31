"""Kleiner Raster-Router (2 Lagen, A*) für die letzten offenen Verbindungen nach Freerouting."""
import heapq, math
import numpy as np
import pcbnew
from PIL import Image, ImageDraw

MM = pcbnew.ToMM
U = pcbnew.FromMM
G = 0.2            # Rastergröße mm (wird für Feinarbeit umgestellt)


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


def island(b, netname, seed):
    """Kupfer, das mit dem Pad 'seed' zusammenhängt – je Lage als Item-Liste (für Ziele)."""
    from shapely.geometry import Polygon as SPoly, LineString as SL, Point as SP
    items = []   # (geom, layers, rec)
    for f in b.GetFootprints():
        for pd in f.Pads():
            if pd.GetNetname() != netname: continue
            ls = [L for L in (pcbnew.F_Cu, pcbnew.B_Cu) if pd.IsOnLayer(L)]
            pp = pad_poly(pd)
            items.append((SPoly(pp).buffer(0.01), ls, ('poly', pp, None), pd))
    for t in b.GetTracks():
        if t.GetNetname() != netname: continue
        if t.GetClass() == 'PCB_VIA':
            c = (MM(t.GetPosition().x), MM(t.GetPosition().y)); r = MM(t.GetWidth()) / 2
            items.append((SP(c).buffer(r), [pcbnew.F_Cu, pcbnew.B_Cu], ('circ', c, r), t))
        else:
            a, e = t.GetStart(), t.GetEnd()
            seg = ((MM(a.x), MM(a.y)), (MM(e.x), MM(e.y)))
            g = SL(seg) if seg[0] != seg[1] else SP(seg[0])
            items.append((g.buffer(MM(t.GetWidth()) / 2), [t.GetLayer()], ('seg', seg, MM(t.GetWidth())), t))
    start = [i for i, it in enumerate(items) if it[3] is seed or (hasattr(it[3], 'GetPosition') and it[0].contains(SP(MM(seed.GetPosition().x), MM(seed.GetPosition().y))) and it[2][0] == 'poly' and it[3].GetParent().GetReference() == seed.GetParent().GetReference() and it[3].GetNumber() == seed.GetNumber())]
    seen = set(start); stack = list(start)
    while stack:
        i = stack.pop()
        for j, it in enumerate(items):
            if j in seen: continue
            if set(items[i][1]) & set(it[1]) and items[i][0].intersects(it[0]):
                seen.add(j); stack.append(j)
    res = {pcbnew.F_Cu: [], pcbnew.B_Cu: []}
    for j in seen:
        for L in items[j][1]:
            res[L].append(items[j][2])
    return res, [items[j][3] for j in seen]


def route(b, netname, src_pad, edge_poly, width=0.3, clear=0.25, via=(0.8, 0.4), margin=25, targets=None):
    """Verbindet src_pad (PAD) mit dem übrigen Kupfer desselben Netzes."""
    sx, sy = MM(src_pad.GetPosition().x), MM(src_pad.GetPosition().y)
    xs = [p[0] for p in edge_poly]; ys = [p[1] for p in edge_poly]
    bx0, by0, bx1, by1 = min(xs), min(ys), max(xs), max(ys)
    if targets is not None:
        tp = [(sx, sy)]
        for L in targets:
            for kind, geo, extra in targets[L]:
                tp += list(geo) if kind in ('poly', 'seg') else [geo]
        m = float(__import__("os").environ.get("RMARGIN", "6"))
        bx0 = max(bx0, min(p[0] for p in tp) - m); by0 = max(by0, min(p[1] for p in tp) - m)
        bx1 = min(bx1, max(p[0] for p in tp) + m); by1 = min(by1, max(p[1] for p in tp) + m)
    g = Grid(b, bx0, by0, bx1, by1)
    layers = [pcbnew.F_Cu, pcbnew.B_Cu]
    blocked, target, vblock = [], [], None
    ownitems = []
    srcpoly = ('poly', pad_poly(src_pad), None)
    for L in layers:
        obst, own = collect(b, netname, L)
        # Platinenrand
        outside = Image.new('1', (g.w, g.h), 1)
        ImageDraw.Draw(outside).polygon([g.px(*p) for p in edge_poly], fill=0)
        ob = g.raster(obst, clear + width / 2 + G * 0.6) | np.array(outside, dtype=bool).T
        er = g.raster([('poly', edge_poly, None)], 0)
        ob |= ~g.raster_inset(edge_poly, 0.6 + width / 2) if hasattr(g, 'raster_inset') else False
        own_wo_src = [o for o in own if not (o[0] == 'poly' and o[1] == srcpoly[1])] if targets is None else targets[L]
        ownitems.append(own_wo_src)
        tg = g.raster(own_wo_src, 0)
        blocked.append(ob)
        target.append(tg & ~ob | tg)
        vb = g.raster(obst, clear + via[0] / 2 + G * 0.6)
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
        ci, cj = g.cell(sx, sy)
        for i, j in zip(ii, jj):
            if not blocked[k][i, j] and abs(i - ci) <= 1 and abs(j - cj) <= 1:
                starts.append((i, j, k))
        if not any(s[2] == k for s in starts):
            starts.append((ci, cj, k)); blocked[k][ci, cj] = False
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
    # letztes Stück sicher ins Zielkupfer führen (Mittelpunkt Pad / nächster Punkt auf der Bahn)
    from shapely.geometry import Point as SP, Polygon as SPoly, LineString as SL
    ex, ey = g.mm(end[0], end[1])
    best, bd = None, 1e9
    for kind, geo, extra in ownitems[end[2]]:
        if kind == 'poly':
            pg = SPoly(geo); dd = pg.distance(SP(ex, ey))
            if dd < bd: bd, best = dd, (pg.centroid.x, pg.centroid.y) if dd > 0 or not pg.contains(SP(ex, ey)) else (ex, ey)
            if pg.contains(SP(ex, ey)) and pg.buffer(-0.05).contains(SP(ex, ey)): bd, best = -1, (ex, ey)
        elif kind == 'seg':
            ln = SL(geo); dd = ln.distance(SP(ex, ey))
            if dd < bd:
                q = ln.interpolate(ln.project(SP(ex, ey))); bd, best = dd, (q.x, q.y)
        elif kind == 'circ':
            dd = math.dist(geo, (ex, ey))
            if dd < bd: bd, best = dd, geo
        if bd < 0: break
    tail = None
    if best is not None and math.dist(best, (ex, ey)) > 0.01:
        tail = ((ex, ey), best)
    # erstes Stück im Padmittelpunkt beginnen
    for n_, (a, c) in enumerate(segs):
        t = pcbnew.PCB_TRACK(b)
        pa = g.mm(a[0], a[1]) if n_ else (sx, sy) if a[2] == path[0][2] else g.mm(a[0], a[1])
        pc = g.mm(c[0], c[1])
        t.SetStart(pcbnew.VECTOR2I(U(pa[0]), U(pa[1]))); t.SetEnd(pcbnew.VECTOR2I(U(pc[0]), U(pc[1])))
        t.SetWidth(U(width)); t.SetLayer(layers[a[2]]); t.SetNet(net)
        b.Add(t)
    if tail:
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(pcbnew.VECTOR2I(U(tail[0][0]), U(tail[0][1]))); t.SetEnd(pcbnew.VECTOR2I(U(float(tail[1][0])), U(float(tail[1][1]))))
        t.SetWidth(U(min(width, 0.3))); t.SetLayer(layers[end[2]]); t.SetNet(net)
        b.Add(t)
    return True
