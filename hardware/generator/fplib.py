"""Eigene Footprints für Trilumag (werden als Trilumag.pretty ins Projekt geschrieben).

Lokales Koordinatensystem aller Kanten-Footprints: x läuft entlang der Platinenkante,
+y zeigt nach außen (zur Kante hin). Der Footprint-Ursprung liegt innerhalb der Platine.
"""
import os

HDR = '(footprint "{n}" (version 20221018) (generator pcbnew) (layer "F.Cu")\n  (descr "{d}")\n  (attr {attr})\n'


def _txt(kind, val, x, y, layer, size=1.0, hide=False):
    h = ' hide' if hide else ''
    return f'  (fp_text {kind} "{val}" (at {x} {y}) (layer "{layer}"){h} (effects (font (size {size} {size}) (thickness {size * 0.15:.2f}))))\n'


def _rect(x1, y1, x2, y2, layer, w=0.12):
    return f'  (fp_rect (start {x1} {y1}) (end {x2} {y2}) (stroke (width {w}) (type default)) (fill none) (layer "{layer}"))\n'


def _line(x1, y1, x2, y2, layer, w=0.12):
    return f'  (fp_line (start {x1} {y1}) (end {x2} {y2}) (stroke (width {w}) (type default)) (layer "{layer}"))\n'


def pogo(name, reverse):
    """5-poliger Magnet-Pogo-Steckverbinder, rechtwinklig, RM 2,54 mm, an der Kante.
    Pin 1 GND, 2 +24V, 3 A, 4 B, 5 Kantenerkennung. Die Buchse ist gespiegelt nummeriert,
    damit sie dem Stecker der Nachbarplatine (deren Kante andersherum läuft) genau gegenübersteht."""
    kind = 'Buchse (weiblich, federnd)' if reverse else 'Stecker (männlich, Pins)'
    s = HDR.format(n=name, d=f'Magnet-Pogo 5P RM2.54 rechtwinklig, {kind}. Maße vor dem Bestellen mit dem Datenblatt abgleichen.', attr='through_hole')
    s += _txt('reference', 'REF**', 0, -3.2, 'F.SilkS')
    s += _txt('value', name, 0, -4.6, 'F.Fab', 0.8)
    s += _rect(-7.5, -1.6, 7.5, 3.0, 'F.Fab', 0.1)          # Körper bis zur Kante
    s += _rect(-8.0, -2.2, 8.0, 3.2, 'F.CrtYd', 0.05)
    s += _line(-7.6, -1.9, 7.6, -1.9, 'F.SilkS')
    for i in range(5):
        num = 5 - i if reverse else i + 1
        x = round(-5.08 + 2.54 * i, 3)
        shape = 'rect' if num == 1 else 'circle'
        s += f'  (pad "{num}" thru_hole {shape} (at {x} 0) (size 1.8 1.8) (drill 1.0) (layers "*.Cu" "*.Mask"))\n'
    # Markierung Pin 1
    x1 = 5.08 if reverse else -5.08
    s += f'  (fp_circle (center {x1} -2.6) (end {x1 + 0.25} -2.6) (stroke (width 0.25) (type default)) (fill solid) (layer "F.SilkS"))\n'
    s += _txt('user', 'B' if reverse else 'S', 6.6 if not reverse else -6.6, -0.0, 'F.SilkS', 0.9)
    return s + ')\n'


def strip(name, start):
    """Lötpads für das Ende eines WS2814-24V-Streifens (12 mm breit, 4 Leiter).
    Pad 1 +24V, 2 Daten (DI bzw. DO), 3 Backup-Daten (BI bzw. BO), 4 GND – quer zum Streifen, RM 3 mm.
    Am Anfangs-Pad liegt der Streifen auf der +x-Seite, am End-Pad auf der -x-Seite."""
    lab = ['24V', 'DI', 'BI', 'GND'] if start else ['24V', 'DO', 'BO', 'GND']
    s = HDR.format(n=name, d='Lötpads WS2814 24V, 12 mm Streifen, ' + ('Streifenanfang' if start else 'Streifenende'), attr='smd')
    tx = -3.6 if start else 3.6
    s += _txt('reference', 'REF**', tx, -7.2, 'F.SilkS', 0.8)
    s += _txt('value', name, 0, 7.4, 'F.Fab', 0.8)
    s += _rect(-2.4, -6.6, 2.4, 6.6, 'F.CrtYd', 0.05)
    ys = [4.5, 1.5, -1.5, -4.5]          # Pad 1 außen (Kantenseite)
    for i, y in enumerate(ys):
        s += f'  (pad "{i + 1}" smd roundrect (at 0 {y}) (size 3.5 2.0) (layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.15))\n'
        just = 'right' if start else 'left'
        s += f'  (fp_text user "{lab[i]}" (at {tx if start else tx} {y}) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12)) (justify {just})))\n'
    return s + ')\n'


def wire6(name, kind):
    """6 Lötaugen für das Kabel zum Magnet-Pogo-Stecker (Stecker sitzt um 90° gedreht im Gehäuse).
    1 GND, 2 A, 3 B, 4 Kantenerkennung, 5 GND, 6 +24V. Pin 1 und 6 für dickere Adern."""
    s = HDR.format(n=name, d=f'Kabelanschluss zum 6-pol. Magnet-Pogo-{kind}, Raster 3,5 mm, Pin 1/6 für AWG20, 2-5 für AWG24-26', attr='through_hole')
    s += _txt('reference', 'REF**', -11.6, 0, 'F.SilkS', 0.9)
    s += _txt('value', name, 0, -4.2, 'F.Fab', 0.8)
    s += _rect(-10.4, -2.0, 10.4, 2.0, 'F.Fab', 0.1)
    s += _rect(-10.8, -2.4, 10.8, 3.6, 'F.CrtYd', 0.05)
    lab = ['GND', 'A', 'B', 'S', 'GND', '24V']
    for i in range(6):
        x = round(-8.75 + 3.5 * i, 3)
        big = i in (0, 5)
        size, drill = (2.6, 1.4) if big else (1.9, 1.0)
        shape = 'rect' if i == 0 else 'circle'
        s += f'  (pad "{i + 1}" thru_hole {shape} (at {x} 0) (size {size} {size}) (drill {drill}) (layers "*.Cu" "*.Mask"))\n'
        s += f'  (fp_text user "{lab[i]}" (at {x} -2.3) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    s += _txt('user', kind, 0, 2.6, 'F.SilkS', 0.8)
    return s + ')\n'


def wire4(name):
    """4 Lötaugen für das Kabel zum ersten LED-Streifen: 24V, DI, BI, GND (Streifen 2 und 3 hängen direkt am Vorgänger)."""
    s = HDR.format(n=name, d='Kabelanschluss LED-Streifen WS2814, Raster 3,5 mm, 24V/GND für AWG20', attr='through_hole')
    s += _txt('reference', 'REF**', -8.1, 0, 'F.SilkS', 0.9)
    s += _txt('value', name, 0, -4.2, 'F.Fab', 0.8)
    s += _rect(-6.9, -2.0, 6.9, 2.0, 'F.Fab', 0.1)
    s += _rect(-7.3, -3.6, 7.3, 2.4, 'F.CrtYd', 0.05)
    lab = ['24V', 'DI', 'BI', 'GND']
    for i in range(4):
        x = round(-5.25 + 3.5 * i, 3)
        big = i in (0, 3)
        size, drill = (2.6, 1.4) if big else (1.9, 1.0)
        shape = 'rect' if i == 0 else 'circle'
        s += f'  (pad "{i + 1}" thru_hole {shape} (at {x} 0) (size {size} {size}) (drill {drill}) (layers "*.Cu" "*.Mask"))\n'
        s += f'  (fp_text user "{lab[i]}" (at {x} -2.3) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))\n'
    return s + ')\n'


def write_lib(dirpath):
    os.makedirs(dirpath, exist_ok=True)
    fps = {
        'Kabel_6P_Stecker': wire6('Kabel_6P_Stecker', 'Stecker'),
        'Kabel_6P_Buchse': wire6('Kabel_6P_Buchse', 'Buchse'),
        'WS2814_Anfang': strip('WS2814_Anfang', True),
        'WS2814_Ende': strip('WS2814_Ende', False),
        'Kabel_4P_LED': wire4('Kabel_4P_LED'),
    }
    for n, t in fps.items():
        open(os.path.join(dirpath, n + '.kicad_mod'), 'w').write(t)
    return list(fps)
