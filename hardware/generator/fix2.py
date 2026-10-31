import pcbnew, subprocess, os, sys, shutil
from board import Builder
import designs
which, nets = sys.argv[1], sys.argv[2].split(',')
d, cfg = getattr(designs, which)()
path = f'out/{d.name}/{d.name}.kicad_pcb'
b = pcbnew.LoadBoard(path)
zs = [z for z in b.Zones() if not z.GetIsRuleArea()]
saved = [(z.GetLayer(),) for z in zs]
for z in zs: b.Remove(z)
for t in list(b.GetTracks()):
    if t.GetNetname() in nets: b.Remove(t)
dsn, ses = '/tmp/fix.dsn', '/tmp/fix.ses'
pcbnew.ExportSpecctraDSN(b, dsn)
t = open(dsn, encoding='utf-8', errors='replace').read().replace('Ω','R').replace('µ','u')
open(dsn,'w').write(''.join(c if ord(c)<128 else '_' for c in t))
if os.path.exists(ses): os.remove(ses)
subprocess.run(['timeout','300','java','-jar','/opt/fr/fr2.jar','--gui.enabled=false','--router.max_passes=60','--router.optimizer.enabled=false','-de',dsn,'-do',ses,'-inc','Masse'],capture_output=True)
# Nur die gewünschten Netze übernehmen
bl = Builder.__new__(Builder); bl.b = b
bl.nets = {n: b.FindNet(n) for n in nets}
bl.import_ses(ses)
pcbnew.SaveBoard(path, b)
# Zonen wieder anlegen und füllen
bl2 = Builder.__new__(Builder); bl2.b = b; bl2.prj = d.name; bl2.out = f'out/{d.name}'
bl2.nets = {n: b.FindNet(n) for n in d.netlist()}
bl2.board_poly = None
bl2.gnd_zones()
b = bl2.b; b.BuildConnectivity()
print('offen:', b.GetConnectivity().GetUnconnectedCount(True))
bl2.save()
