"""Erzeugt KiCad-7-Projekte (Schaltplan + Platine) für Trilumag aus einer Bauteilliste.

Schaltplan: jedes Bauteil mit Netz-Labels an allen Pins (elektrisch eindeutig, ERC-fähig).
Platine: Footprints aus den KiCad-Bibliotheken bzw. eigene (Pogo-Stecker, LED-Streifen-Pads),
Netze direkt aus derselben Liste, Kontur als Dreieck, Massefläche, Routing mit Freerouting.
"""
import math, os, uuid, re, subprocess, json, shutil
import pcbnew
from symlib import *

FPDIR = '/usr/share/kicad/footprints'
U = pcbnew.FromMM


def uid():
    return str(uuid.uuid4())


class Part:
    def __init__(self, ref, lib, sym, value, fp, nets, at=None, rot=0, side='F', group='', note='', near=None, padnets=None, mpn=''):
        self.ref, self.lib, self.sym, self.value, self.fp = ref, lib, sym, value, fp
        self.nets = nets          # Pinname oder -nummer -> Netz
        self.at, self.rot, self.side, self.group, self.note = at, rot, side, group, note
        self.near = near          # (Ref, Pad) – automatisch daneben platzieren
        self.padnets = padnets or {}   # zusätzliche Footprint-Pads ohne Symbolpin
        self.mpn = mpn            # Bestellbezeichnung für die Stückliste
        self.uuid = uid()


class Design:
    def __init__(self, name, title):
        self.name, self.title = name, title
        self.parts = []
        self.texts = []           # (x, y, text) im Schaltplan
        self.power = {'GND', '+24V', '+24V_ROH', '+3V3', '+5V', 'VBUS', 'VIN', 'VIN_F'}

    def add(self, *a, **k):
        p = Part(*a, **k)
        self.parts.append(p)
        return p

    # ---------- Pinzuordnung ----------
    def pinmap(self, p):
        fs = flat(p.lib, p.sym)
        pl = pins(fs)
        res = {}
        for key, net in p.nets.items():
            hit = [q for q in pl if q['num'] == key] or [q for q in pl if q['name'] == key]
            if not hit:
                raise KeyError(f'{p.ref}: Pin {key} gibt es in {p.lib}:{p.sym} nicht ({[(q["num"], q["name"]) for q in pl]})')
            for q in hit:
                res[q['num']] = net
        return fs, pl, res

    def netlist(self):
        nets = {}
        for p in self.parts:
            _, _, m = self.pinmap(p)
            for num, net in m.items():
                nets.setdefault(net, []).append((p.ref, num))
        return nets

    # ---------- Schaltplan ----------
    def write_sch(self, path):
        root = uid()
        libs = {}
        body = []
        # Gruppen nebeneinander anordnen
        groups = []
        for p in self.parts:
            if p.group not in groups:
                groups.append(p.group)
        gx, gy, rowh = 25.4, 30.48, 0
        maxw = 380
        pwr_needed = set()
        drivers = set()
        for g in groups:
            gparts = [p for p in self.parts if p.group == g]
            # Blockbreite schätzen
            sizes = []
            for p in gparts:
                fs, pl, m = self.pinmap(p)
                xs = [q['x'] for q in pl] or [0]
                ys = [q['y'] for q in pl] or [0]
                w = max(xs) - min(xs) + 30
                h = max(ys) - min(ys) + 20
                sizes.append((p, fs, pl, m, w, h, min(xs), max(ys)))
            bw = min(maxw, sum(s[4] for s in sizes) + 10)
            if gx + bw > maxw + 25:
                gx = 25.4
                gy += rowh + 25
                rowh = 0
            body.append(f'  (text "{g}" (at {gx:.2f} {gy - 8:.2f} 0) (effects (font (size 2.5 2.5) bold) (justify left bottom)) (uuid {uid()}))')
            x, y, lineh = gx, gy, 0
            for (p, fs, pl, m, w, h, minx, maxy) in sizes:
                if x + w > gx + bw + 1:
                    x = gx
                    y += lineh + 12
                    lineh = 0
                sx = round((x - minx + 10) / 2.54) * 2.54
                sy = round((y + maxy + 8) / 2.54) * 2.54
                lid = p.lib + ':' + p.sym
                libs[lid] = fs
                body.append(self.sym_instance(p, fs, pl, m, sx, sy, root, pwr_needed, drivers))
                x += w + 10
                lineh = max(lineh, h + 12)
            rowh = max(rowh, y + lineh - gy)
            gx += bw + 20
        # PWR_FLAG für Versorgungsnetze ohne Treiber
        fx, fy = 25.4, gy + rowh + 30
        flag = flat('power', 'PWR_FLAG')
        for i, net in enumerate(sorted(pwr_needed - drivers)):
            libs['power:PWR_FLAG'] = flag
            body.append(self.flag_instance(net, fx, fy, root, i + 1))
            fx += 25.4
        for (tx, ty, t) in self.texts:
            body.append(f'  (text "{t}" (at {tx} {ty} 0) (effects (font (size 1.8 1.8)) (justify left bottom)) (uuid {uid()}))')
        libtxt = '\n'.join('    ' + dump(v, 2) for v in libs.values())
        out = f'''(kicad_sch (version 20230121) (generator eeschema)
  (uuid {root})
  (paper "A2")
  (title_block (title "{self.title}") (rev "0.2") (company "Trilumag")
    (comment 1 "Erzeugt aus der Bauteilliste; Netze über Labels verbunden"))
  (lib_symbols
{libtxt}
  )
{chr(10).join(body)}
  (sheet_instances (path "/" (page "1")))
)
'''
        open(path, 'w', encoding='utf-8').write(out)

    def sym_instance(self, p, fs, pl, m, sx, sy, root, pwr_needed, drivers):
        u = p.uuid
        props = {pp[1][1]: pp for pp in kids(fs, 'property')}
        lines = [f'  (symbol (lib_id "{p.lib}:{p.sym}") (at {sx:.2f} {sy:.2f} 0) (unit 1)',
                 f'    (in_bom yes) (on_board yes) (dnp no) (uuid {u})']
        ys = [q['y'] for q in pl]
        top = sy - (max(ys) if ys else 0) - 3
        bot = sy - (min(ys) if ys else 0) + 3
        fields = [('Reference', p.ref, top - 2.5), ('Value', p.value, bot + 1.5), ('Footprint', p.fp, bot + 4), ('Datasheet', '~', bot + 4)]
        for i, (k, v, yy) in enumerate(fields):
            hide = ' hide' if k in ('Footprint', 'Datasheet') else ''
            lines.append(f'    (property "{k}" "{v}" (at {sx:.2f} {yy:.2f} 0) (effects (font (size 1.27 1.27)){hide}))')
        if p.mpn:
            lines.append(f'    (property "Bestellnr" "{p.mpn}" (at {sx:.2f} {bot + 6.5:.2f} 0) (effects (font (size 1.27 1.27)) hide))')
        if p.note:
            lines.append(f'    (property "Hinweis" "{p.note}" (at {sx:.2f} {bot + 6.5:.2f} 0) (effects (font (size 1.27 1.27)) hide))')
        for q in pl:
            lines.append(f'    (pin "{q["num"]}" (uuid {uid()}))')
        lines.append(f'    (instances (project "{self.name}" (path "/{root}" (reference "{p.ref}") (unit 1))))')
        lines.append('  )')
        # Labels an jedem Pin
        for q in pl:
            px, py = sx + q['x'], sy - q['y']
            a = math.radians(q['rot'])
            dx, dy = -math.cos(a), math.sin(a)          # nach außen, in Schaltplan-Koordinaten
            ex, ey = px + dx * 2.54, py + dy * 2.54
            net = m.get(q['num'])
            if net is None:
                lines.append(f'  (no_connect (at {px:.2f} {py:.2f}) (uuid {uid()}))')
                continue
            if q['type'] in ('power_in',) and net:
                pwr_needed.add(net)
            if q['type'] in ('power_out',):
                drivers.add(net)
            lines.append(f'  (wire (pts (xy {px:.2f} {py:.2f}) (xy {ex:.2f} {ey:.2f})) (stroke (width 0) (type default)) (uuid {uid()}))')
            ang = 180 if dx < -0.5 else 0 if dx > 0.5 else (90 if dy < 0 else 270)
            just = 'right' if ang in (180, 270) else 'left'
            lines.append(f'  (label "{net}" (at {ex:.2f} {ey:.2f} {ang}) (fields_autoplaced) (effects (font (size 1.27 1.27)) (justify {just} bottom)) (uuid {uid()}))')
        return '\n'.join(lines)

    def flag_instance(self, net, x, y, root, n):
        u = uid()
        return (f'  (symbol (lib_id "power:PWR_FLAG") (at {x:.2f} {y:.2f} 0) (unit 1) (in_bom yes) (on_board yes) (dnp no) (uuid {u})\n'
                f'    (property "Reference" "#FLG{n:02d}" (at {x:.2f} {y - 5:.2f} 0) (effects (font (size 1.27 1.27)) hide))\n'
                f'    (property "Value" "PWR_FLAG" (at {x:.2f} {y - 4:.2f} 0) (effects (font (size 1.27 1.27))))\n'
                f'    (property "Footprint" "" (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) hide))\n'
                f'    (pin "1" (uuid {uid()}))\n'
                f'    (instances (project "{self.name}" (path "/{root}" (reference "#FLG{n:02d}") (unit 1)))))\n'
                f'  (wire (pts (xy {x:.2f} {y:.2f}) (xy {x:.2f} {y + 2.54:.2f})) (stroke (width 0) (type default)) (uuid {uid()}))\n'
                f'  (label "{net}" (at {x:.2f} {y + 2.54:.2f} 270) (effects (font (size 1.27 1.27)) (justify right bottom)) (uuid {uid()}))')
