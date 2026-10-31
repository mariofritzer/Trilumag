import sys, re, pcbnew
path = sys.argv[1]
def fill_drc():
    b = pcbnew.LoadBoard(path)
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    pcbnew.WriteDRCReport(b, '/tmp/c.txt', pcbnew.EDA_UNITS_MILLIMETRES, True)
    return open('/tmp/c.txt').read()
for it in range(4):
    t = fill_drc()
    dang = re.findall(r'\[(?:track|via)_dangling\][^\n]*\n[^\n]*\n\s*@\(([\d.]+) mm, ([\d.]+) mm\): (Track|Via) \[([^\]]+)\]', t)
    if not dang: break
    b = pcbnew.LoadBoard(path)
    items = []
    for tr in b.GetTracks():
        if tr.GetClass() == 'PCB_VIA':
            p = tr.GetPosition(); items.append((tr, 'Via', [(pcbnew.ToMM(p.x), pcbnew.ToMM(p.y))]))
        else:
            a, c = tr.GetStart(), tr.GetEnd()
            items.append((tr, 'Track', [(pcbnew.ToMM(a.x), pcbnew.ToMM(a.y)), (pcbnew.ToMM(c.x), pcbnew.ToMM(c.y))]))
    rm = []
    for x, y, kind, net in dang:
        px, py = float(x), float(y)
        for tr, k, pts in items:
            if k == kind and tr.GetNetname() == net and not tr.IsLocked() and min(abs(q[0] - px) + abs(q[1] - py) for q in pts) < 0.03 and tr not in rm:
                rm.append(tr); break
    for tr in rm: b.Remove(tr)
    pcbnew.SaveBoard(path, b)
    s = open(path).read().replace('(paper "A4")', '(paper "A3")', 1); open(path, 'w').write(s)
    print('entfernt', len(rm))
    if not rm: break
