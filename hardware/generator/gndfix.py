import sys, pcbnew, router
from board import outline
P = sys.argv[1]; router.G = float(sys.argv[4]) if len(sys.argv) > 4 else 0.1
b = pcbnew.LoadBoard(P); pcbnew.ZONE_FILLER(b).Fill(b.Zones())
fps = {f.GetReference(): f for f in b.GetFootprints()}
src_ref, src_num = sys.argv[2].split('.'); dst_ref, dst_num = sys.argv[3].split('.')
pd = [p for p in fps[src_ref].Pads() if p.GetNumber() == src_num][0]
dp = [p for p in fps[dst_ref].Pads() if p.GetNumber() == dst_num][0]
tg, mem = router.island(b, pd.GetNetname(), dp)
ok = False
for w, c in ((0.4, 0.25), (0.3, 0.2)):
    ok = router.route(b, pd.GetNetname(), pd, outline(), width=w, clear=c, targets=tg)
    if ok: break
print(ok)
pcbnew.SaveBoard(P, b)
s = open(P).read().replace('(paper "A4")', '(paper "A3")', 1); open(P, 'w').write(s)
