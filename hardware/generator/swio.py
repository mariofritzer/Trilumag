import sys, pcbnew, router
from board import outline
P = sys.argv[1]; router.G = 0.05
b = pcbnew.LoadBoard(P)
U = lambda v: int(round(v * 1e6)); V = lambda x, y: pcbnew.VECTOR2I(U(x), U(y))
fps = {f.GetReference(): f for f in b.GetFootprints()}
N = {n: b.FindNet(n) for n in ('SWIO', 'TINT')}
pd = [p for p in fps['J5'].Pads() if p.GetNumber() == '1'][0]
for t in []:
    b.Remove(t)
for a, c in (((177.8625, 144.375), (179.6, 144.375)),):
    t = pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(U(0.25)); t.SetLayer(pcbnew.F_Cu); t.SetNet(N['SWIO']); t.SetLocked(True); b.Add(t)
v = pcbnew.PCB_VIA(b); v.SetPosition(V(179.6, 144.375)); v.SetWidth(U(0.7)); v.SetDrill(U(0.35)); v.SetNet(N['SWIO'])
v.SetViaType(pcbnew.VIATYPE_THROUGH); v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); v.SetLocked(True); b.Add(v)
class Src:
    def __init__(s, x, y, layers): s.x, s.y, s.L = x, y, layers
    def GetPosition(s): return V(s.x, s.y)
    def IsOnLayer(s, L): return L in s.L
_pp = router.pad_poly
router.pad_poly = lambda pd: [(pd.x - .1, pd.y - .1), (pd.x + .1, pd.y - .1), (pd.x + .1, pd.y + .1), (pd.x - .1, pd.y + .1)] if isinstance(pd, Src) else _pp(pd)
tg, _ = router.island(b, 'SWIO', pd)
print('SWIO', router.route(b, 'SWIO', Src(179.6, 144.375, (pcbnew.B_Cu,)), outline(), width=0.3, clear=0.2, targets=tg))
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
