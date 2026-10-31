"""Gleichnamige Pins (Sicherungsclip, Taster, Steckerreihe) und USB-Stummel mit dem Router verbinden."""
import sys, pcbnew, router
from board import outline
P = sys.argv[1]; router.G = 0.05
b = pcbnew.LoadBoard(P); pcbnew.ZONE_FILLER(b).Fill(b.Zones())
fps = {f.GetReference(): f for f in b.GetFootprints()}
V = lambda x, y: pcbnew.VECTOR2I(int(round(x * 1e6)), int(round(y * 1e6)))
class Src:
    def __init__(s, x, y, layers): s.x, s.y, s.L = x, y, layers
    def GetPosition(s): return V(s.x, s.y)
    def IsOnLayer(s, L): return L in s.L
_pp = router.pad_poly
router.pad_poly = lambda pd: [(pd.x - .1, pd.y - .1), (pd.x + .1, pd.y - .1), (pd.x + .1, pd.y + .1), (pd.x - .1, pd.y + .1)] if isinstance(pd, Src) else _pp(pd)
def pads(ref, num): return [p for p in fps[ref].Pads() if p.GetNumber() == num]
def go(net, src, dst, w=0.3):
    tg, mem = router.island(b, net, dst)
    for ww, c in ((w, 0.2), (0.25, 0.2)):
        if router.route(b, net, src, outline(), width=ww, clear=c, targets=tg):
            return True
    return False
F, B = pcbnew.F_Cu, pcbnew.B_Cu
jobs = []
for a in sys.argv[2:]:
    jobs.append(a)
for j in jobs:
    k = j.split(':')
    if k[0] == 'pair':            # pair:REF:NUM:NET:width
        ps = pads(k[1], k[2]); print(j, go(k[3], ps[0], ps[1], float(k[4])), flush=True)
    elif k[0] == 'pad':           # pad:REF:NUM:DREF:DNUM:width
        s = pads(k[1], k[2])[0]; d = pads(k[3], k[4])[0]; print(j, go(s.GetNetname(), s, d, float(k[5])), flush=True)
    elif k[0] == 'src':           # src:NET:x:y:F|B:DREF:DNUM:width
        L = (F,) if k[4] == 'F' else (B,)
        d = pads(k[5], k[6])[0]; print(j, go(k[1], Src(float(k[2]), float(k[3]), L), d, float(k[7])), flush=True)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
