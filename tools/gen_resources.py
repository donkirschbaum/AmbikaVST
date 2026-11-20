import sys,re
src,ns,out=sys.argv[1:]
t=open(src).read()
t=re.sub(r'#include "[^"]*resources.h"','#include "resources.h"\n\n#define U16(x) static_cast<uint16_t>(x)\n#define U8(x) static_cast<uint8_t>(x)',t)
t=t.replace('namespace ambika {','namespace ambika {\nnamespace %s {'%ns,1)
t=t.replace('}  // namespace ambika','}  // namespace %s\n}  // namespace ambika'%ns)
t=t.replace('PROGMEM','')
res=[];cur='U16'
for l in t.split('\n'):
    m=re.search(r'const prog_(u?int)(8|16)_t',l)
    if m: cur='U8' if m.group(2)=='8' else 'U16'
    if '"' not in l and not l.strip().startswith('//') and not l.startswith('#'):
        l=re.sub(r'(?<![\w)])-(\d+)', cur+r'(-\1)', l)
    res.append(l)
hdr="// Lookup tables from the Ambika firmware, Copyright Emilie Gillet, GPL v3.\n// Converted for desktop compilers by tools/gen_resources.py\n"
open(out,'w').write(hdr+'\n'.join(res))
