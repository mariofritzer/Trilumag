import sys, pcbnew, router
from board import outline
P = sys.argv[1]; router.G = 0.1
b = pcbnew.LoadBoard(P)
U = lambda v: int(round(v * 1e6)); V = lambda x, y: pcbnew.VECTOR2I(U(x), U(y))
fps = {f.GetReference(): f for f in b.GetFootprints()}
N = {n: b.FindNet(n) for n in ('GND', 'SDA', 'EN', 'BOOT')}
def trk(net, pts, w=0.25):
    for a, c in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(b); t.SetStart(V(*a)); t.SetEnd(V(*c)); t.SetWidth(U(w)); t.SetLayer(pcbnew.F_Cu); t.SetNet(N[net]); t.SetLocked(True); b.Add(t)
# U6: Pin 7 (GND) unter dem Gehäuse zu Pin 2/1
trk('GND', [(190.19, 196.39), (190.19, 197.6), (189.19, 198.6), (189.19, 200.79), (188.69, 200.79)])
# U6: SDA-Stummel nach außen
trk('SDA', [(190.19, 200.79), (190.19, 202.6)])
class Src:
    def __init__(s, x, y, layers): s.x, s.y, s.L = x, y, layers
    def GetPosition(s): return V(s.x, s.y)
    def IsOnLayer(s, L): return L in s.L
_pp = router.pad_poly
router.pad_poly = lambda pd: [(pd.x - .1, pd.y - .1), (pd.x + .1, pd.y - .1), (pd.x + .1, pd.y + .1), (pd.x - .1, pd.y + .1)] if isinstance(pd, Src) else _pp(pd)
def pad(ref, num, near=None):
    ps = [p for p in fps[ref].Pads() if p.GetNumber() == num]
    if near: ps.sort(key=lambda p: abs(p.GetPosition().y / 1e6 - near[1]) + abs(p.GetPosition().x / 1e6 - near[0]))
    return ps
tg, _ = router.island(b, 'SDA', pad('U3', '39')[0])
print('SDA', router.route(b, 'SDA', Src(190.19, 202.6, (pcbnew.F_Cu,)), outline(), width=0.25, clear=0.2, targets=tg))
for ref, net in (('SW1', 'BOOT'), ('SW2', 'EN')):
    ps = pad(ref, '1')
    # Zielinsel: die des anderen Pins mit gleicher Nummer
    a = ps[0]
    for other in ps[1:]:
        tg, mem = router.island(b, net, other)
        if any(m is a or (m.GetClass() == 'PAD' and m.GetPosition() == a.GetPosition()) for m in mem):
            continue
        print(ref, router.route(b, net, a, outline(), width=0.3, clear=0.2, targets=tg))
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
