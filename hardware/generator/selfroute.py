"""Ganze Platine mit dem eigenen Raster-Router verlegen (statt Freerouting)."""
import sys, os, shutil, math, time
import pcbnew
import designs, make, router
MM = pcbnew.ToMM
from board import Builder, outline

which = sys.argv[1]
router.G = float(os.environ.get('GRID', router.G))
d, cfg = getattr(designs, which)()
out = os.path.join(make.OUT, d.name + '-self')
if os.path.exists(out): shutil.rmtree(out)
os.makedirs(out)
d.write_sch(os.path.join(out, d.name + '.kicad_sch'))
make.project_file(os.path.join(out, d.name + '.kicad_pro'), d.name, cfg)
shutil.copy(os.path.join(make.OUT, d.name, 'fp-lib-table'), out) if os.path.exists(os.path.join(make.OUT, d.name, 'fp-lib-table')) else None
b = Builder(d, out, cfg)
b.setup_rules(); b.build_parts(); b.assign_classes(); b.decorate()
b.power_ring()
print('Masse-Vias', b.gnd_fanout())
B = b.b
t0 = time.time()
power = set(cfg['classmap'].get('Power', []))
nets = {n: v for n, v in d.netlist().items() if n != 'GND' and len(v) > 1}
fps = {f.GetReference(): f for f in B.GetFootprints()}

def pad(ref, num):
    return [p for p in fps[ref].Pads() if p.GetNumber() == num][0]

# kurze Netze zuerst, Versorgung zuletzt
def span(n):
    ps = [pad(r, k).GetPosition() for r, k in nets[n]]
    return max(math.hypot(pcbnew.ToMM(a.x - c.x), pcbnew.ToMM(a.y - c.y)) for a in ps for c in ps)
tight = set(os.environ.get('FIRST', '').split(','))
order = sorted(nets, key=lambda n: (n in power, not any(r in tight for r, _ in nets[n]), span(n)))
fail = []
for n in order:
    w, c = (2.0, 0.3) if n in power else (0.3, 0.2)
    pins = nets[n]
    # Reihenfolge: nächster Nachbar ab dem ersten Pad
    seed = pad(*pins[0])
    rest = pins[1:]
    while rest:
        tg, members = router.island(B, n, seed)
        rest = [p for p in rest if not any(m is pad(*p) or (m.GetClass() == 'PAD' and m.GetParent().GetReference() == p[0] and m.GetNumber() == p[1]) for m in members)]
        if not rest: break
        def dist(p):
            a = pad(*p).GetPosition()
            return min(math.hypot(MM(a.x) - (x[1][0][0] if x[0] == 'poly' else x[1][0] if x[0] == 'circ' else x[1][0][0]), MM(a.y) - (x[1][0][1] if x[0] == 'poly' else x[1][1] if x[0] == 'circ' else x[1][0][1])) for L in tg for x in tg[L])
        rest.sort(key=dist)
        p = rest.pop(0)
        ok = False
        for ww, cc in ((w, c),) + (((1.0, 0.3),) if n in power else ()) + ((0.25, 0.2),):
            ok = router.route(B, n, pad(*p), outline(), width=ww, clear=cc, targets=tg)
            if ok: break
        if not ok: fail.append((n, p))
print('Zeit %.0f s, fehlgeschlagen: %s' % (time.time() - t0, fail))
b.gnd_zones()
print('Stitching-Vias', b.stitch())
print('offen', b.unconnected())
p = b.save()
make.bom(d, os.path.join(out, 'stueckliste.csv'))
make.fix_pro(os.path.join(out, d.name + '.kicad_pro'), cfg)
print(p)
