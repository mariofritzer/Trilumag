import pcbnew, sys, re, collections
b = pcbnew.LoadBoard(sys.argv[1])
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
rpt = sys.argv[2]
pcbnew.WriteDRCReport(b, rpt, pcbnew.EDA_UNITS_MILLIMETRES, True)
t = open(rpt).read()
blocks = re.split(r'\n(?=\[)', t)
c = collections.Counter()
for bl in blocks:
    m = re.match(r'\[(\w+)\]', bl)
    if not m or m.group(1) == 'lib_footprint_issues': continue
    sev = 'error' if 'Severity: error' in bl else 'warning'
    c[(sev, m.group(1))] += 1
    if sev == 'error' and '-v' in sys.argv: print(bl[:400])
for k, v in sorted(c.items()): print(v, *k)
