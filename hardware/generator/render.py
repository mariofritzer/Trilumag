import subprocess, sys, cairosvg, os
def render(pcb, png, layers='F.Cu,B.Cu,F.SilkS,Edge.Cuts', w=1600):
    svg = png[:-4] + '.svg'
    subprocess.run(['kicad-cli','pcb','export','svg','-l',layers,'--page-size-mode','2','--exclude-drawing-sheet','-o',svg,pcb],capture_output=True)
    cairosvg.svg2png(url=svg, write_to=png, output_width=w, background_color='white')
    os.remove(svg)
if __name__=='__main__':
    render(sys.argv[1], sys.argv[2], *(sys.argv[3:4] or []))
