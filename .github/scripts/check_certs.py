"""Prüft, ob die echten Zertifikatsketten der Update-Server mit cacerts.h aufgehen.
Ergänzt vorher DigiCert Global Root CA, falls der Build-Rechner es kennt. Bricht den Build ab, wenn nicht."""
import os, re, subprocess, sys

H = 'firmware/trilumag/cacerts.h'
s = open(H).read()
extra = '/etc/ssl/certs/DigiCert_Global_Root_CA.pem'
if os.path.exists(extra) and 'DigiCert Global Root CA\n' not in s:
    pem = open(extra).read().strip().splitlines()
    s = s.rstrip().rstrip(';') + '\n  // DigiCert Global Root CA\n' + '\n'.join('  "%s\\n"' % l for l in pem) + ';\n'
    open(H, 'w').write(s)
lines = re.findall(r'"([^"]*)\\n"', s)
open('/tmp/ca.pem', 'w').write('\n'.join(lines) + '\n')
print(f"{s.count('BEGIN CERTIFICATE')} Stammzertifikate in cacerts.h")

ok = True
for host in ['mariofritzer.github.io', 'github.com', 'release-assets.githubusercontent.com', 'objects.githubusercontent.com']:
    r = subprocess.run(['openssl', 's_client', '-connect', f'{host}:443', '-servername', host, '-CAfile', '/tmp/ca.pem', '-showcerts'],
                       input='', capture_output=True, text=True, timeout=30)
    out = r.stdout + r.stderr
    if 'Verify return code: 0' in out:
        print(f'{host}: gültig')
    else:
        ok = False
        print(f'::error::Zertifikat von {host} lässt sich mit cacerts.h nicht prüfen')
        for l in out.splitlines():
            if re.match(r'\s*\d+ s:|\s*i:', l):
                print(f'::error::{host} {l.strip()}')
sys.exit(0 if ok else 1)
