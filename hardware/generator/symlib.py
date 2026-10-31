"""Kleine Hilfen zum Lesen der KiCad-Symbolbibliotheken (KiCad 7)."""
import re, os
SYMDIR='/usr/share/kicad/symbols'

def tokenize(s):
    i=0;n=len(s)
    while i<n:
        c=s[i]
        if c in ' \t\r\n': i+=1; continue
        if c=='(' or c==')': yield c; i+=1; continue
        if c=='"':
            j=i+1; b=[]
            while s[j]!='"':
                if s[j]=='\\': b.append(s[j+1]); j+=2; continue
                b.append(s[j]); j+=1
            yield ('S',''.join(b)); i=j+1; continue
        j=i
        while j<n and s[j] not in ' \t\r\n()': j+=1
        yield ('A',s[i:j]); i=j

def parse(s):
    st=[[]]
    for t in tokenize(s):
        if t=='(': st.append([])
        elif t==')': x=st.pop(); st[-1].append(x)
        else: st[-1].append(t)
    return st[0][0]

def dump(x,ind=0):
    """S-Ausdruck wieder als Text (mehrzeilig, kompakt genug)."""
    if isinstance(x,tuple):
        if x[0]=='S': return '"'+x[1].replace('\\','\\\\').replace('"','\\"')+'"'
        return x[1]
    head=x[0]
    simple=all(not isinstance(e,list) for e in x)
    if simple: return '('+' '.join(dump(e) for e in x)+')'
    out='('+dump(head)
    for e in x[1:]:
        if isinstance(e,list): out+='\n'+'  '*(ind+1)+dump(e,ind+1)
        else: out+=' '+dump(e)
    return out+')'

def A(v): return ('A',str(v))
def S(v): return ('S',str(v))
def name(x): return x[0][1] if isinstance(x,list) and x and isinstance(x[0],tuple) else None
def kids(x,tag): return [e for e in x if isinstance(e,list) and name(e)==tag]
def kid(x,tag):
    k=kids(x,tag); return k[0] if k else None

_cache={}
def library(lib):
    if lib not in _cache:
        _cache[lib]=parse(open(os.path.join(SYMDIR,lib+'.kicad_sym'),encoding='utf-8').read())
    return _cache[lib]

def find(lib,sym):
    L=library(lib)
    for s in kids(L,'symbol'):
        if s[1][1]==sym: return s
    raise KeyError(lib+':'+sym)

def flat(lib,sym):
    """Symbol mit aufgelöstem extends, als eigenständiges Symbol namens lib:sym."""
    s=find(lib,sym)
    ext=kid(s,'extends')
    if ext:
        parent=flat(lib,ext[1][1])
        props=kids(s,'property')
        pn={p[1][1] for p in props}
        body=[e for e in parent[2:] if not (isinstance(e,list) and name(e)=='property' and e[1][1] in pn)]
        res=[A('symbol'),S(lib+':'+sym)]
        res+=[e for e in body if not (isinstance(e,list) and name(e)=='symbol')]
        res+=props
        # Untersymbole umbenennen
        pname=parent[1][1].split(':')[-1]
        for e in body:
            if isinstance(e,list) and name(e)=='symbol':
                sub=[A('symbol'),S(e[1][1].replace(pname,sym,1))]+e[2:]
                res.append(sub)
        return res
    res=[A('symbol'),S(lib+':'+sym)]+s[2:]
    return res

def pins(fs):
    """Pins eines (flachen) Symbols: Liste (nummer, name, x, y, winkel, typ, unit)."""
    out=[]
    for sub in kids(fs,'symbol'):
        m=re.search(r'_(\d+)_(\d+)$',sub[1][1]); unit=int(m.group(1)) if m else 1
        for p in kids(sub,'pin'):
            at=kid(p,'at'); nm=kid(p,'name'); nu=kid(p,'number'); ln=kid(p,'length')
            out.append(dict(num=nu[1][1],name=nm[1][1],x=float(at[1][1]),y=float(at[2][1]),rot=float(at[3][1]) if len(at)>3 else 0,
                            type=p[1][1],unit=unit,len=float(ln[1][1])))
    return out
