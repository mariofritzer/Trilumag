"""Platinenaufbau: Dreieck 200 mm, Footprints platzieren, Freerouting, Masseflächen, DRC."""
import math, os, re, subprocess, json, shutil
import pcbnew
from shapely.geometry import Polygon, Point, box
from shapely.ops import unary_union
from shapely import affinity
import fplib

FPDIR = '/usr/share/kicad/footprints'
U = pcbnew.FromMM
MM = pcbnew.ToMM
L = 200.0
H = L * math.sqrt(3) / 2
OX, OY = 110.0, 62.0           # Platine mittig auf A3
A, B, C = (OX, OY + H), (OX + L, OY + H), (OX + L / 2, OY)
G = (OX + L / 2, OY + H * 2 / 3)


def P(x, y):
    """Koordinate relativ zur linken oberen Ecke des Dreieck-Umrechtecks."""
    return (OX + x, OY + y)


def readable(r):
    r = (r + 180) % 360 - 180
    if r > 90: r -= 180
    if r <= -90: r += 180
    return r
EDGES = [(A, B), (B, C), (C, A)]


def tvec(k):  # Einheitsvektor entlang Kante k
    p0, p1 = EDGES[k - 1]
    return ((p1[0] - p0[0]) / L, (p1[1] - p0[1]) / L)


def ept(k, s, d):
    """Punkt auf Kante k (1..3), s mm vom Kantenanfang, d mm nach innen."""
    p0, _ = EDGES[k - 1]
    t = tvec(k)
    n = (t[1], -t[0])
    return (p0[0] + t[0] * s + n[0] * d, p0[1] + t[1] * s + n[1] * d)


def erot(k):
    t = tvec(k)
    return math.degrees(math.atan2(-t[1], t[0]))


def toward(v, dist):
    dx, dy = v[0] - G[0], v[1] - G[1]
    l = math.hypot(dx, dy)
    return (G[0] + dx / l * dist, G[1] + dy / l * dist)


def xform(pts, at, rot):
    a = math.radians(rot)
    c, s = math.cos(a), math.sin(a)
    return [(at[0] + x * c + y * s, at[1] - x * s + y * c) for x, y in pts]


def outline(chamfer=5.0):
    pts = []
    for k in (1, 2, 3):
        pts.append(ept(k, chamfer, 0))
        pts.append(ept(k, L - chamfer, 0))
    return pts


def strip_zone(k):
    return Polygon([ept(k, 44, 6), ept(k, 156, 6), ept(k, 156, 20.5), ept(k, 44, 20.5)])


class Builder:
    def __init__(self, design, outdir, cfg):
        self.d, self.out, self.cfg = design, outdir, cfg
        self.prj = design.name
        os.makedirs(outdir, exist_ok=True)
        self.fplibdir = os.path.join(outdir, 'Trilumag.pretty')
        fplib.write_lib(self.fplibdir)
        self.b = pcbnew.BOARD()
        self.b.SetCopperLayerCount(2)
        tb = self.b.GetTitleBlock()
        tb.SetTitle(design.title); tb.SetRevision('0.2'); tb.SetCompany('Trilumag')
        tb.SetComment(0, 'Dreieck 200 mm Kantenlänge, 2 Lagen, Handlöt-Ausführung')
        self.nets = {}
        self.fps = {}
        self.polys = {}
        self.warn = []

    # ---------- Grundlagen ----------
    def net(self, n):
        if n not in self.nets:
            ni = pcbnew.NETINFO_ITEM(self.b, n)
            self.b.Add(ni)
            self.nets[n] = ni
        return self.nets[n]

    def load(self, fpid):
        lib, name = fpid.split(':', 1)
        path = self.fplibdir if lib == 'Trilumag' else os.path.join(FPDIR, lib + '.pretty')
        f = pcbnew.FootprintLoad(path, name)
        if f is None:
            raise RuntimeError('Footprint fehlt: ' + fpid)
        f.SetFPID(pcbnew.LIB_ID(lib, name))
        return f

    def local_rect(self, f):
        """Begrenzungsrechteck in Footprint-Koordinaten (Courtyard, sonst alles ohne Text)."""
        f.SetPosition(pcbnew.VECTOR2I(0, 0))
        f.SetOrientationDegrees(0)
        xs, ys = [], []
        for g in f.GraphicalItems():
            if g.GetLayerName() == 'F.CrtYd' and g.GetClass() != 'PCB_TEXT':
                bb = g.GetBoundingBox()
                xs += [MM(bb.GetX()), MM(bb.GetRight())]
                ys += [MM(bb.GetY()), MM(bb.GetBottom())]
        if not xs:
            bb = f.GetBoundingBox(False, False)
            xs = [MM(bb.GetX()), MM(bb.GetRight())]
            ys = [MM(bb.GetY()), MM(bb.GetBottom())]
        return min(xs), min(ys), max(xs), max(ys)

    def setup_rules(self):
        ds = self.b.GetDesignSettings()
        ns = ds.m_NetSettings
        dc = ns.m_DefaultNetClass
        dc.SetTrackWidth(U(0.3)); dc.SetClearance(U(0.2)); dc.SetViaDiameter(U(0.8)); dc.SetViaDrill(U(0.4))
        self.classes = {}
        for name, tw, cl, vd, vdr in self.cfg['classes']:
            nc = pcbnew.NETCLASS(name)
            nc.SetTrackWidth(U(tw)); nc.SetClearance(U(cl)); nc.SetViaDiameter(U(vd)); nc.SetViaDrill(U(vdr))
            ns.m_NetClasses[name] = nc
            self.classes[name] = nc
        ds.m_MinClearance = U(0.2)
        ds.m_TrackMinWidth = U(0.25)
        ds.m_ViasMinSize = U(0.6)
        ds.m_MinThroughDrill = U(0.3)
        ds.m_HoleClearance = U(0.25)
        ds.m_CopperEdgeClearance = U(0.5)

    # ---------- Platzierung ----------
    def build_parts(self):
        d = self.d
        self.board_poly = Polygon(outline())
        forb = [strip_zone(k) for k in (1, 2, 3)] + [Polygon(p) for p in self.cfg.get('reserve', [])]
        self.forbidden = unary_union(forb)
        self.allowed = self.board_poly.buffer(-1.2)
        placed = []
        order = [p for p in d.parts if p.at is not None] + [p for p in d.parts if p.at is None]
        for p in order:
            fs, pl, m = d.pinmap(p)
            f = self.load(p.fp)
            x1, y1, x2, y2 = self.local_rect(f)
            rect = [(x1, y1), (x2, y1), (x2, y2), (x1, y2)]
            if p.at is not None:
                at, rot = p.at, p.rot
                poly = Polygon(xform(rect, at, rot))
                for q, pp in placed:
                    if poly.buffer(0.1).intersects(pp) and poly.intersection(pp).area > 0.3:
                        self.warn.append(f'{p.ref} überlappt {q}')
                if not self.allowed.buffer(1.2).contains(poly):
                    self.warn.append(f'{p.ref} ragt über die Platine')
            else:
                tref, tpad = p.near
                tf = self.fps[tref]
                tp = [MM(x) for x in next(pd for pd in tf.Pads() if pd.GetNumber() == str(tpad)).GetPosition()]
                at, rot, poly = self.find_spot(rect, tp, placed, p.rot)
            f.SetPosition(pcbnew.VECTOR2I(U(at[0]), U(at[1])))
            f.SetOrientationDegrees(rot)
            f.SetReference(p.ref)
            f.SetValue(p.value)
            f.SetPath(pcbnew.KIID_PATH('/' + p.uuid))
            for pd in f.Pads():
                n = pd.GetNumber()
                net = m.get(n, p.padnets.get(n))
                if net:
                    pd.SetNet(self.net(net))
            if p.ref.startswith('U3') and 'ESP32' in p.sym:
                self.esp_handsolder(f)
            self.b.Add(f)
            self.fps[p.ref] = f
            placed.append((p.ref, poly))
            self.polys[p.ref] = poly
        for ref, (fpid, at) in self.cfg.get('holes', {}).items():
            f = self.load(fpid)
            f.SetPosition(pcbnew.VECTOR2I(U(at[0]), U(at[1])))
            f.SetReference(ref); f.SetValue('M3')
            for pd in f.Pads():
                pd.SetNet(self.net('GND'))
            self.b.Add(f)

    def find_spot(self, rect, target, placed, rot0):
        occ = unary_union([pp for _, pp in placed]).buffer(1.5)
        okarea = self.allowed.difference(self.forbidden)
        rots = [rot0, (rot0 + 90) % 360] if rot0 is not None else [0, 90]
        r = 0.0
        while r < 70:
            n = max(1, int(2 * math.pi * r / 0.8))
            best = None
            for i in range(n):
                a = 2 * math.pi * i / n
                c = (target[0] + r * math.cos(a), target[1] + r * math.sin(a))
                for rot in rots:
                    poly = Polygon(xform(rect, c, rot))
                    if okarea.contains(poly) and not poly.intersects(occ):
                        return c, rot, poly
            r += 0.5
        raise RuntimeError('kein Platz gefunden')

    def esp_handsolder(self, f):
        """Masse-Pad des ESP32-S3: winzige Wärmevias durch ein 1,5-mm-Loch ersetzen,
        damit man das Pad von der Rückseite mit dem Lötkolben verlöten kann."""
        for pd in list(f.Pads()):
            if pd.GetNumber() == '41' and pd.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
                f.Remove(pd)
        smd = [pd for pd in f.Pads() if pd.GetNumber() == '41'][0]
        hole = pcbnew.PAD(f)
        hole.SetNumber('41')
        hole.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
        hole.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        hole.SetSize(pcbnew.VECTOR2I(U(2.4), U(2.4)))
        hole.SetDrillSize(pcbnew.VECTOR2I(U(1.5), U(1.5)))
        ls = pcbnew.LSET.AllCuMask()
        ls.AddLayer(pcbnew.B_Mask)
        hole.SetLayerSet(ls)
        hole.SetPosition(smd.GetPosition())
        hole.SetPos0(smd.GetPos0()) if hasattr(hole, "SetPos0") else None
        f.Add(hole)

    # ---------- Kontur, Sperrflächen, Beschriftung ----------
    @staticmethod
    def cu2():
        ls = pcbnew.LSET()
        ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu)
        return ls

    def add_line(self, a, b, layer, w=0.1):
        s = pcbnew.PCB_SHAPE(self.b)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(U(a[0]), U(a[1])))
        s.SetEnd(pcbnew.VECTOR2I(U(b[0]), U(b[1])))
        s.SetLayer(layer)
        s.SetWidth(U(w))
        self.b.Add(s)

    def add_poly_lines(self, pts, layer, w=0.1):
        for i in range(len(pts)):
            self.add_line(pts[i], pts[(i + 1) % len(pts)], layer, w)

    def add_text(self, txt, at, rot=0, size=1.5, layer=pcbnew.F_SilkS):
        t = pcbnew.PCB_TEXT(self.b)
        t.SetText(txt)
        t.SetPosition(pcbnew.VECTOR2I(U(at[0]), U(at[1])))
        t.SetTextAngleDegrees(rot)
        t.SetTextSize(pcbnew.VECTOR2I(U(size), U(size)))
        t.SetTextThickness(U(size * 0.15))
        t.SetLayer(layer)
        self.b.Add(t)

    def place_refs(self):
        """Bestückungsdruck: Referenzen waagrecht neben das Bauteil setzen, ohne Pads/andere Texte zu überdecken."""
        obst = []
        for f in self.b.GetFootprints():
            for pd in f.Pads():
                bb = pd.GetBoundingBox()
                obst.append(box(MM(bb.GetX()), MM(bb.GetY()), MM(bb.GetRight()), MM(bb.GetBottom())).buffer(0.25))
        pads = unary_union(obst)
        texts = []
        edge = self.board_poly.buffer(-0.8)
        for ref, f in self.fps.items():
            if ref[0] in 'XLH':
                continue
            t = f.Reference()
            sz = 0.9
            t.SetTextSize(pcbnew.VECTOR2I(U(sz), U(sz)))
            t.SetTextThickness(U(0.14))
            w, h = 0.72 * sz * len(ref) + 0.3, sz + 0.3
            x1, y1, x2, y2 = self.polys[ref].bounds
            cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
            cands = [(cx, y1 - h / 2 - 0.2), (cx, y2 + h / 2 + 0.2), (x1 - w / 2 - 0.2, cy), (x2 + w / 2 + 0.2, cy),
                     (x1 - w / 2, y1 - h / 2), (x2 + w / 2, y1 - h / 2), (x1 - w / 2, y2 + h / 2), (x2 + w / 2, y2 + h / 2)]
            best = None
            for d in (0, 0.8, 1.6, 2.6):
                for k, (x, y) in enumerate(cands):
                    dx = (x - cx); dy = (y - cy); l = math.hypot(dx, dy) or 1
                    x, y = x + dx / l * d, y + dy / l * d
                    tb = box(x - w / 2, y - h / 2, x + w / 2, y + h / 2)
                    if edge.contains(tb) and not tb.intersects(pads) and not any(tb.intersects(o) for o in texts):
                        best = (x, y, tb); break
                if best: break
            if best is None:
                t.SetVisible(False)
                continue
            t.SetTextAngleDegrees(0) if hasattr(t, 'SetTextAngleDegrees') else None
            t.SetPosition(pcbnew.VECTOR2I(U(best[0]), U(best[1])))
            texts.append(best[2])

    def edge_keepout(self, w=1.0):
        """Schmaler Sperrstreifen an der Kontur, damit der Autorouter Abstand zur Kante hält."""
        o = outline()
        inner = list(Polygon(o).buffer(-w, join_style=2).exterior.coords)[:-1]
        # inneres Polygon hat dieselbe Eckenzahl; Startpunkt passend suchen
        j0 = min(range(len(inner)), key=lambda j: math.dist(inner[j], o[0]))
        inner = inner[j0:] + inner[:j0]
        if math.dist(inner[1], o[1]) > math.dist(inner[-1], o[1]):
            inner = [inner[0]] + inner[1:][::-1]
        n = len(o)
        for i in range(n):
            pts = [o[i], o[(i + 1) % n], inner[(i + 1) % n], inner[i]]
            z = pcbnew.ZONE(self.b)
            z.SetIsRuleArea(True)
            z.SetLayerSet(self.cu2())
            z.SetDoNotAllowCopperPour(False); z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True)
            z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
            ol = z.Outline(); ol.NewOutline()
            for x, y in pts:
                ol.Append(U(x), U(y))
            z.SetZoneName('Kantenabstand')
            self.b.Add(z)

    def decorate(self):
        self.add_poly_lines(outline(), pcbnew.Edge_Cuts, 0.1)
        self.edge_keepout()
        self.place_refs()
        for k in (1, 2, 3):
            z = [ept(k, 44, 7), ept(k, 156, 7), ept(k, 156, 19), ept(k, 44, 19)]
            self.add_poly_lines(z, pcbnew.F_SilkS, 0.12)
            r = readable(erot(k))
            self.add_text(f'Kante {k} · LED-Streifen {k} · WS2814 24 V · 10 cm', ept(k, 100, 13), r, 1.2)
        for txt, at, size in self.cfg.get('texts', []):
            self.add_text(txt, at, 0, size)
        for pts in self.cfg.get('keepouts', []):
            z = pcbnew.ZONE(self.b)
            z.SetIsRuleArea(True)
            z.SetLayerSet(self.cu2())
            z.SetDoNotAllowCopperPour(True); z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True)
            z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
            ol = z.Outline(); ol.NewOutline()
            for x, y in pts:
                ol.Append(U(x), U(y))
            z.SetZoneName('Antenne')
            self.b.Add(z)

    def assign_classes(self):
        for name, nets in self.cfg['classmap'].items():
            for n in nets:
                if n in self.nets:
                    self.nets[n].SetNetClass(self.classes[name])

    # ---------- Vorverlegte 24-V-Schiene ----------
    def track(self, a, b, net, w, layer, lock=True):
        t = pcbnew.PCB_TRACK(self.b)
        t.SetStart(pcbnew.VECTOR2I(U(a[0]), U(a[1])))
        t.SetEnd(pcbnew.VECTOR2I(U(b[0]), U(b[1])))
        t.SetWidth(U(w)); t.SetLayer(layer); t.SetNet(net)
        t.SetLocked(lock)
        self.b.Add(t)

    def power_ring(self, depth=8.5, w=2.0):
        """+24V als Ring auf der Unterseite, 8,5 mm innerhalb der Kante (unter den LED-Streifen).
        Pogo-Pin 2 und die 24-V-Pads der Streifen werden mit kurzen Stichen angebunden."""
        rc = self.cfg.get('ring')
        if not rc:
            return
        net = self.net('+24V')
        inset = lambda v: (v[0] + (G[0] - v[0]) / math.dist(v, G) * 2 * depth, v[1] + (G[1] - v[1]) / math.dist(v, G) * 2 * depth)
        if rc == 'closed':
            pts = [inset(A), inset(B), inset(C), inset(A)]
            edges = (1, 2, 3)
        else:
            pts = [ept(2, 20, depth), inset(C), ept(3, 180, depth)]
            edges = (2, 3)
        for a, b in zip(pts, pts[1:]):
            self.track(a, b, net, w, pcbnew.B_Cu)
        for k in edges:
            for ref, f in self.fps.items():
                if not (ref.startswith(f'X{k}') or ref.startswith(f'L{k}')):
                    continue
                for pd in f.Pads():
                    if pd.GetNetname() != '+24V':
                        continue
                    p = (MM(pd.GetPosition().x), MM(pd.GetPosition().y))
                    t = tvec(k)
                    n = (t[1], -t[0])
                    p0 = EDGES[k - 1][0]
                    s_ = (p[0] - p0[0]) * t[0] + (p[1] - p0[1]) * t[1]
                    if ref.startswith('X'):
                        self.track(p, ept(k, s_, depth), net, w, pcbnew.B_Cu)
                    else:
                        sv = s_ - 2.7 if ref.endswith('S') else s_ + 2.7
                        v = ept(k, sv, depth)
                        self.track(p, v, net, w, pcbnew.F_Cu)
                        self.via(v, net, 1.2, 0.6).SetLocked(True)

    def gnd_fanout(self):
        """Jedes SMD-Massepad bekommt eine eigene Via zur Massefläche (GND wird nicht geroutet)."""
        from shapely.geometry import LineString
        gnd = self.net('GND')
        obst = []
        for f in self.b.GetFootprints():
            for pd in f.Pads():
                if pd.GetNetname() == 'GND' and pd.GetAttribute() == pcbnew.PAD_ATTRIB_SMD:
                    continue
                bb = pd.GetBoundingBox()
                obst.append(box(MM(bb.GetX()), MM(bb.GetY()), MM(bb.GetRight()), MM(bb.GetBottom())).buffer(0.3))
        for t in self.b.GetTracks():
            a, b = t.GetStart(), t.GetEnd()
            g = LineString([(MM(a.x), MM(a.y)), (MM(b.x), MM(b.y))]) if a != b else Point(MM(a.x), MM(a.y))
            obst.append(g.buffer(MM(t.GetWidth()) / 2 + 0.3))
        for z in self.b.Zones():
            if z.GetIsRuleArea() and z.GetDoNotAllowVias():
                ol = z.Outline().Outline(0)
                obst.append(Polygon([(MM(ol.CPoint(i).x), MM(ol.CPoint(i).y)) for i in range(ol.PointCount())]))
        occ = unary_union(obst)
        inside = self.board_poly.buffer(-1.5)
        vias = []
        n = 0
        for ref, f in self.fps.items():
            fc = (MM(f.GetPosition().x), MM(f.GetPosition().y))
            for pd in f.Pads():
                if pd.GetNetname() != 'GND' or pd.GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
                    continue
                p = (MM(pd.GetPosition().x), MM(pd.GetPosition().y))
                bb = pd.GetBoundingBox()
                half = max(MM(bb.GetWidth()), MM(bb.GetHeight())) / 2
                own = box(MM(bb.GetX()), MM(bb.GetY()), MM(bb.GetRight()), MM(bb.GetBottom()))
                if any(own.buffer(0.1).contains(Point(v)) or own.distance(Point(v)) < 1.5 for v in vias):
                    continue          # großes Pad hat schon eine Via
                dx, dy = p[0] - fc[0], p[1] - fc[1]
                dirs = [(math.cos(a), math.sin(a)) for a in [i * math.pi / 4 for i in range(8)]]
                dirs.sort(key=lambda d: -(d[0] * dx + d[1] * dy))
                done = False
                for dist in [half + 0.75 + 0.25 * i for i in range(10)]:
                    for d in dirs:
                        v = (p[0] + d[0] * dist, p[1] + d[1] * dist)
                        vc = Point(v).buffer(0.4 + 0.25)
                        ln = LineString([p, v]).buffer(0.25 + 0.2).difference(own.buffer(0.05))
                        if not inside.contains(vc) or vc.intersects(occ) or ln.intersects(occ):
                            continue
                        if any(math.dist(v, w) < 1.1 for w in vias):
                            continue
                        self.track(p, v, gnd, 0.5, pcbnew.F_Cu)
                        self.via(v, gnd, 0.8, 0.4).SetLocked(True)
                        vias.append(v)
                        occ = occ.union(Point(v).buffer(0.4 + 0.3))
                        n += 1
                        done = True
                        break
                    if done:
                        break
                if not done:
                    self.warn.append(f'keine Masse-Via für {ref}.{pd.GetNumber()}')
        return n

    # ---------- Routing ----------
    def route(self, passes=30):
        dsn = os.path.join(self.out, self.prj + '.dsn')
        ses = os.path.join(self.out, self.prj + '.ses')
        if not pcbnew.ExportSpecctraDSN(self.b, dsn):
            raise RuntimeError('DSN-Export fehlgeschlagen')
        t = open(dsn, encoding='utf-8', errors='replace').read()
        t = t.replace('Ω', 'R').replace('µ', 'u')
        t = ''.join(ch if ord(ch) < 128 else '_' for ch in t)
        open(dsn, 'w').write(t)
        if os.path.exists(ses):
            os.remove(ses)
        cmd = ['timeout', '480', 'java', '-jar', '/opt/fr/fr2.jar', '--gui.enabled=false', '--router.max_passes=150',
               '--router.optimizer.enabled=false', '-de', dsn, '-do', ses, '-inc', 'Masse']
        reuse = os.environ.get('SES_REUSE')
        if reuse:
            shutil.copy(reuse, ses)
            r = subprocess.CompletedProcess(cmd, 0, '', '')
        else:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=1700)
        self.frlog = r.stdout + r.stderr
        open('/tmp/fr_' + self.prj + '.log', 'w').write(self.frlog)
        if not os.path.exists(ses):
            print(r.stdout[-3000:], r.stderr[-3000:])
            raise RuntimeError('Freerouting lieferte keine SES-Datei')
        self.import_ses(ses)
        shutil.copy(ses, '/tmp/last.ses'); shutil.copy(dsn, '/tmp/last.dsn')
        os.remove(dsn); os.remove(ses)

    def import_ses(self, path):
        txt = open(path).read()
        m = re.search(r'\(resolution\s+(\w+)\s+(\d+)\)', txt)
        unit, res = m.group(1), int(m.group(2))
        scale = {'um': 1e-3, 'mm': 1.0, 'mil': 0.0254, 'inch': 25.4}[unit] / res
        lay = {'F.Cu': pcbnew.F_Cu, 'B.Cu': pcbnew.B_Cu}
        i = txt.find('(network_out')
        body = txt[i:]
        for nm in re.finditer(r'\(net\s+("[^"]*"|\S+)(.*?)(?=\(net\s|\Z)', body, re.S):
            netname = nm.group(1).strip('"')
            net = self.nets.get(netname)
            if net is None:
                continue
            chunk = nm.group(2)
            for w in re.finditer(r'\(path\s+(\S+)\s+([\d.]+)\s+([-\d.\s]+)\)', chunk):
                layer = lay[w.group(1)]
                width = float(w.group(2)) * scale
                c = [float(v) for v in w.group(3).split()]
                pts = [(c[j] * scale, -c[j + 1] * scale) for j in range(0, len(c) - 1, 2)]
                for a, b in zip(pts, pts[1:]):
                    t = pcbnew.PCB_TRACK(self.b)
                    t.SetStart(pcbnew.VECTOR2I(U(a[0]), U(a[1])))
                    t.SetEnd(pcbnew.VECTOR2I(U(b[0]), U(b[1])))
                    t.SetWidth(U(width)); t.SetLayer(layer); t.SetNet(net)
                    self.b.Add(t)
            for v in re.finditer(r'\(via\s+"?([^"\s]+)"?\s+([-\d.]+)\s+([-\d.]+)', chunk):
                ps = v.group(1)
                mm = re.search(r'_(\d+):(\d+)_um', ps)
                dia, drl = (int(mm.group(1)) / 1000, int(mm.group(2)) / 1000) if mm else (0.8, 0.4)
                self.via((float(v.group(2)) * scale, -float(v.group(3)) * scale), net, dia, drl)

    def via(self, p, net, dia=0.8, drl=0.4):
        v = pcbnew.PCB_VIA(self.b)
        v.SetPosition(pcbnew.VECTOR2I(U(p[0]), U(p[1])))
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetWidth(U(dia)); v.SetDrill(U(drl)); v.SetNet(net)
        self.b.Add(v)
        return v

    # ---------- Masseflächen ----------
    def gnd_zones(self):
        pts = Polygon(outline()).buffer(-0.6, join_style=2).exterior.coords[:-1]
        self.zones = []
        for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
            z = pcbnew.ZONE(self.b)
            z.SetLayer(layer)
            z.SetNet(self.net('GND'))
            ol = z.Outline(); ol.NewOutline()
            for x, y in pts:
                ol.Append(U(x), U(y))
            z.SetLocalClearance(U(0.35))
            z.SetMinThickness(U(0.3))
            z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
            z.SetThermalReliefGap(U(0.4))
            z.SetThermalReliefSpokeWidth(U(0.5))
            z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
            z.SetZoneName('GND')
            self.b.Add(z)
        self.reload()
        self.fill()

    def reload(self):
        """Speichern und neu laden – der Zonenfüller stürzt auf frisch im Speicher gebauten Platinen ab."""
        path = self.save()
        self.b = pcbnew.LoadBoard(path)
        self.nets = {n: self.b.FindNet(n) for n in self.d.netlist()} if hasattr(self, 'd') else {n: self.b.FindNet(n) for n in self.nets}
        self.zones = sorted([z for z in self.b.Zones() if not z.GetIsRuleArea()], key=lambda z: z.GetLayer())

    def fill(self):
        self.b.BuildConnectivity()
        f = pcbnew.ZONE_FILLER(self.b)
        f.Fill(self.b.Zones())

    def filled_shape(self, z, layer):
        ps = z.GetFilledPolysList(layer)
        polys = []
        for i in range(ps.OutlineCount()):
            o = ps.Outline(i)
            ext = [(MM(o.CPoint(j).x), MM(o.CPoint(j).y)) for j in range(o.PointCount())]
            holes = []
            for h in range(ps.HoleCount(i)):
                hh = ps.Hole(i, h)
                holes.append([(MM(hh.CPoint(j).x), MM(hh.CPoint(j).y)) for j in range(hh.PointCount())])
            if len(ext) >= 3:
                polys.append(Polygon(ext, holes).buffer(0))
        return unary_union(polys)

    def stitch(self, pitch=9.0):
        top = self.filled_shape(self.zones[0], pcbnew.F_Cu).buffer(-0.75)
        bot = self.filled_shape(self.zones[1], pcbnew.B_Cu).buffer(-0.75)
        both = top.intersection(bot)
        gnd = self.net('GND')
        n = 0
        y = OY + 2.0
        row = 0
        while y < OY + H:
            x = (pitch / 2 if row % 2 else 0) + OX + 2.0
            while x < OX + L:
                if both.contains(Point(x, y)):
                    self.via((x, y), gnd, 0.8, 0.4)
                    n += 1
                x += pitch
            y += pitch * 0.866
            row += 1
        # zusätzlich je eine Via neben jedem Masse-Pad auf nur einer Lage (SMD), falls dort Fläche ist
        self.fill()
        return n

    # ---------- Prüfen, Speichern ----------
    def unconnected(self):
        self.b.BuildConnectivity()
        c = self.b.GetConnectivity()
        return c.GetUnconnectedCount(True)

    def save(self):
        path = os.path.join(self.out, self.prj + '.kicad_pcb')
        pcbnew.SaveBoard(path, self.b)
        t = open(path).read().replace('(paper "A4")', '(paper "A3")', 1)
        open(path, 'w').write(t)
        return path

    def drc(self):
        rpt = os.path.join(self.out, 'drc.txt')
        pcbnew.WriteDRCReport(self.b, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
        return rpt
