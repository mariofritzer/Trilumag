import sys, os, pcbnew, router
from board import outline
P = sys.argv[1]; router.G = float(sys.argv[2]) if len(sys.argv) > 2 else 0.1
b = pcbnew.LoadBoard(P)
fps = {f.GetReference(): f for f in b.GetFootprints()}
class Src:
    def __init__(s, x, y, layers): s.x, s.y, s.L = x, y, layers
    def GetPosition(s): return pcbnew.VECTOR2I(int(s.x * 1e6), int(s.y * 1e6))
    def IsOnLayer(s, L): return L in s.L
_pp = router.pad_poly
router.pad_poly = lambda pd: [(pd.x - .1, pd.y - .1), (pd.x + .1, pd.y - .1), (pd.x + .1, pd.y + .1), (pd.x - .1, pd.y + .1)] if isinstance(pd, Src) else _pp(pd)
F, B = pcbnew.F_Cu, pcbnew.B_Cu
jobs = [('CC1', 281.25, 223.5, (F,), 'R5', '1', 0.3), ('CC2', 287.25, 223.5, (F,), 'R6', '1', 0.3),
        ('USB_DN', 283.25, 223.0, (F,), 'U3', '13', 0.25), ('USB_DP', 284.75, 223.0, (F,), 'U3', '14', 0.25),
        ('VBUS', 284.0, 226.4, (B,), 'D3', '2', 0.4)]
only = sys.argv[3].split(',') if len(sys.argv) > 3 else None
for net, x, y, L, ref, num, w in jobs:
    if only and net not in only: continue
    pd = [p for p in fps[ref].Pads() if p.GetNumber() == num][0]
    tg, mem = router.island(b, net, pd)
    ok = False
    for c in (0.2, 0.18):
        ok = router.route(b, net, Src(x, y, L), outline(), width=w, clear=c, targets=tg)
        if ok: break
    print(net, ok, flush=True)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
