#!/bin/bash
# Hauptplatine: eigener Router, dann Reparaturrunden
cd /home/claude/pcb
P=out/trilumag-hauptplatine-self/trilumag-hauptplatine.kicad_pcb
python3 selfroute.py main 2>&1 | grep -v "ragt\|leak" | tail -4
for i in 1 2 3; do
  echo -n "TX" > /tmp/prio.txt
  RMARGIN=8 timeout 900 python3 repair.py $P 0.05 2>&1 | grep -v leak | tail -1
  python3 drc.py $P /tmp/dm.txt 2>&1 | grep -q unconnected || break
  cp $P /tmp/main_r$i.kicad_pcb
  timeout 900 python3 ripup.py $P $((i-1)) 2>&1 | grep -v leak | tail -1
done
python3 clean.py $P 2>&1 | grep entfernt
python3 drc.py $P /tmp/dm.txt 2>&1 | grep -v assert
echo FERTIG
