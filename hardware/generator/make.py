import sys, os, json, subprocess, shutil
import pcbnew
import designs
from board import Builder

OUT = '/home/claude/pcb/out'


def project_file(path, name, cfg):
    cls = [{"name": "Default", "track_width": 0.3, "clearance": 0.2, "via_diameter": 0.8, "via_drill": 0.4,
            "microvia_diameter": 0.3, "microvia_drill": 0.1, "diff_pair_width": 0.2, "diff_pair_gap": 0.25,
            "diff_pair_via_gap": 0.25, "wire_width": 6, "bus_width": 12, "line_style": 0, "pcb_color": "rgba(0, 0, 0, 0.000)",
            "schematic_color": "rgba(0, 0, 0, 0.000)"}]
    for n, tw, cl, vd, vdr in cfg['classes']:
        c = dict(cls[0]); c.update(name=n, track_width=tw, clearance=cl, via_diameter=vd, via_drill=vdr)
        cls.append(c)
    pats = [{"netclass": k, "pattern": n} for k, v in cfg['classmap'].items() for n in v]
    pro = {
        "board": {"design_settings": {"defaults": {"board_outline_line_width": 0.1, "copper_line_width": 0.2},
                                      "rules": {"min_clearance": 0.2, "min_track_width": 0.2, "min_via_diameter": 0.6,
                                                "min_through_hole_diameter": 0.3, "min_hole_clearance": 0.15,
                                                "min_copper_edge_clearance": 0.5},
                                      "rule_severities": {"starved_thermal": "warning", "silk_overlap": "warning", "silk_over_copper": "warning", "lib_footprint_mismatch": "warning"},
                                      "track_widths": [0.0, 0.3, 0.5, 1.0, 2.0], "via_dimensions": [{"diameter": 0.0, "drill": 0.0}, {"diameter": 0.8, "drill": 0.4}, {"diameter": 1.2, "drill": 0.6}]}},
        "meta": {"filename": name + ".kicad_pro", "version": 1},
        "net_settings": {"classes": cls, "meta": {"version": 3}, "net_colors": None, "netclass_assignments": None,
                         "netclass_patterns": pats},
        "pcbnew": {"last_paths": {}, "page_layout_descr_file": ""},
        "schematic": {"legacy_lib_dir": "", "legacy_lib_list": []},
        "sheets": [], "text_variables": {}
    }
    json.dump(pro, open(path, 'w'), indent=2)


def build(which, route=True):
    d, cfg = getattr(designs, which)()
    out = os.path.join(OUT, d.name)
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(out)
    d.write_sch(os.path.join(out, d.name + '.kicad_sch'))
    project_file(os.path.join(out, d.name + '.kicad_pro'), d.name, cfg)
    open(os.path.join(out, 'fp-lib-table'), 'w').write(
        '(fp_lib_table\n  (version 7)\n  (lib (name "Trilumag")(type "KiCad")(uri "${KIPRJMOD}/Trilumag.pretty")(options "")(descr "Trilumag-Footprints"))\n)\n')
    b = Builder(d, out, cfg)
    b.setup_rules()
    b.build_parts()
    b.assign_classes()
    b.decorate()
    for w in b.warn:
        print('WARNUNG', w)
    if route:
        b.power_ring()
        print('Masse-Vias', b.gnd_fanout())
        b.route()
        b.gnd_zones()
        n = b.stitch()
        print('Stitching-Vias', n)
    print('offene Verbindungen:', b.unconnected())
    p = b.save()
    bom(d, os.path.join(out, 'stueckliste.csv'))
    fix_pro(os.path.join(out, d.name + '.kicad_pro'), cfg)
    return b, p



def fix_pro(path, cfg):
    """KiCad überschreibt beim Speichern die Projektdatei – Netzklassen-Zuordnung und Regelschärfen wieder eintragen."""
    p = json.load(open(path))
    p['net_settings']['netclass_patterns'] = [{"netclass": k, "pattern": n} for k, v in cfg['classmap'].items() for n in v]
    rs = p.setdefault('board', {}).setdefault('design_settings', {}).setdefault('rule_severities', {})
    rs.update({"starved_thermal": "warning", "silk_overlap": "warning", "silk_over_copper": "warning", "lib_footprint_issues": "warning"})
    json.dump(p, open(path, 'w'), indent=2)


def bom(d, path):
    import csv
    rows = {}
    for p in d.parts:
        key = (p.value, p.fp.split(':')[1], p.mpn)
        rows.setdefault(key, []).append(p.ref)
    def nat(r):
        import re
        m = re.match(r'([A-Z]+)(\d+)(.*)', r)
        return (m.group(1), int(m.group(2)), m.group(3)) if m else (r, 0, '')
    out = sorted(rows.items(), key=lambda kv: nat(sorted(kv[1], key=nat)[0]))
    with open(path, 'w', newline='', encoding='utf-8') as f:
        w = csv.writer(f, delimiter=';')
        w.writerow(['Menge', 'Referenzen', 'Wert', 'Footprint', 'Bestellhinweis'])
        for (val, fp, mpn), refs in out:
            w.writerow([len(refs), ' '.join(sorted(refs, key=nat)), val, fp, mpn])


def single_nets(d):
    return {n: v for n, v in d.netlist().items() if len(v) < 2}


if __name__ == '__main__':
    which = sys.argv[1]
    route = '--noroute' not in sys.argv
    b, p = build(which, route)
    print(p)
