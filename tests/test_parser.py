"""Exercise the actual C parser, including malformed untrusted metadata tables."""
from pathlib import Path
import json, struct, subprocess, shutil, tempfile, argparse
ROOT = Path(__file__).resolve().parents[1]
(ROOT / 'build').mkdir(exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--recovered', type=Path)
args = parser.parse_args()
exe = ROOT / 'build/pkg_probe.exe'
subprocess.run([shutil.which('clang'), '-O2', '-Wall', '-Wextra', '-Werror',
               str(ROOT / 'tests/pkg_probe.c'), '-o', str(exe)], check=True)
results = []
def make_pkg(items):
    base = 256
    names = bytearray(b'\0')
    positions = []
    for name, data, flags in items:
        positions.append(len(names)); names += name.encode() + b'\0'
    count = len(items)+1
    table_off = 128
    data_off = table_off + count*32
    table = bytearray(struct.pack('>8I', 0x200, 0, 0x40000000, 0, data_off, len(names), 0, 0))
    body = bytearray(names)
    for i, (name, data, flags) in enumerate(items):
        table += struct.pack('>8I', 0x1000+i, positions[i], flags, 0, data_off+len(body), len(data), 0, 0)
        body += data
    h = bytearray(base+table_off)
    h[:4] = b'\x7fFIH';struct.pack_into('<Q', h, 0x58, base)
    h[base:base+4] = b'\x7fCNT'
    struct.pack_into('>I', h, base+0x10, count)
    struct.pack_into('>I', h, base+0x18, table_off)
    return h+table+body
with tempfile.TemporaryDirectory() as tmp:
    sample = Path(tmp)/'fixture.pkg'
    def check(label, blob, expected, expected_names=None):
        sample.write_bytes(blob)
        p = subprocess.run([str(exe), str(sample)], capture_output=True, text=True)
        assert (p.returncode == 0) == expected, (label, p.returncode, p.stdout, p.stderr)
        if expected_names is not None:
            assert sorted(line.split()[0] for line in p.stdout.splitlines()) == sorted(expected_names), label
        results.append(label)
    param = b'{"titleId":"PPSA12345"}\n'
    good = make_pkg([('param.json',param,0),('icon0.png',b'png-data',0x08000000),
                     ('playgo-chunk.dat',b'chunk',0x08000000),
                     ('playgo-hash-table.dat',b'hash',0x08000000),
                     ('playgo-ficm.dat',b'ficm',0x08000000),
                     ('trophy2/npbind.dat',b'trophy-bind',0x08000000),
                     ('trophy2/trophy00.ucp',b'trophy',0x08000000),
                     ('uds/npbind.dat',b'uds-bind',0x08000000),
                     ('uds/uds00.ucp',b'uds',0x08000000)])
    check('valid launch metadata',good,True,['param.json','icon0.png',
          'playgo-chunk.dat','playgo-hash-table.dat','playgo-ficm.dat',
          'trophy2/npbind.dat','trophy2/trophy00.ucp','uds/npbind.dat','uds/uds00.ucp'])
    check('truncated header',good[:120],False)
    b=good.copy();b[0]=0;check('wrong FIH',b,False)
    b=good.copy();struct.pack_into('<Q',b,0x58,2**64-1);check('overflow CNT offset',b,False)
    b=good.copy();struct.pack_into('>I',b,256+0x10,65536);check('excessive entry count',b,False)
    b=good.copy();struct.pack_into('>I',b,384+32+16,0xffffffff);check('entry outside package',b,False)
    b=good.copy();struct.pack_into('>I',b,384+32+4,0xffffffff);check('invalid name offset',b,False)
    check('encrypted param rejected',make_pkg([('param.json',param,0x80000000)]),False)
    check('unknown metadata flags rejected',make_pkg([('param.json',param,0x100)]),False)
    check('duplicate param rejected',make_pkg([('param.json',param,0),('param.json',param,0)]),False)
    check('no param rejected',make_pkg([('icon0.png',b'png',0)]),False)
    check('path traversal ignored',make_pkg([('param.json',param,0),('../icon.png',b'x',0)]),True,['param.json'])
    check('nested path traversal ignored',make_pkg([('param.json',param,0),('trophy2/../evil.ucp',b'x',0)]),True,['param.json'])
    check('unapproved nested directory ignored',make_pkg([('param.json',param,0),('other/file.ucp',b'x',0)]),True,['param.json'])
    check('encrypted optional image ignored',make_pkg([('param.json',param,0),('icon0.png',b'x',0x80000000)]),True,['param.json'])
    # Exact CNT uint32 sizes, with a virtual file extent; no multi-GB fixture.
    for size in [53313776, 64*1024*1024+1, 300*1024*1024, 0xffffffff]:
        b=make_pkg([('param.json',param,0),('trophy2/trophy00.ucp',b'x',0x08000000)])
        entry=384+2*32
        off=struct.unpack_from('>I',b,entry+16)[0]
        struct.pack_into('>I',b,entry+20,size)
        sample.write_bytes(b)
        extent=256+off+size
        p=subprocess.run([str(exe),str(sample),'--table-only',str(extent)],capture_output=True,text=True)
        assert p.returncode==0,(size,p.stderr)
        assert 'trophy2/trophy00.ucp '+str(size) in p.stdout
        results.append('metadata size without artificial cap '+str(size))
        p=subprocess.run([str(exe),str(sample),'--table-only',str(extent-1)],capture_output=True,text=True)
        assert p.returncode==1 and 'outside PKG' in p.stderr,(size,p.stderr)
        results.append('large metadata still bounded by PKG '+str(size))
    if args.recovered:
        rows=json.loads((args.recovered/'PACKAGES.json').read_text(encoding='utf-8'))
        for row in rows:
            folder=args.recovered/'metadata'/row['title_id'];items=[]
            for e in row['entries']:
                name=e['name'];path=folder/name
                if name and path.is_file() and path.suffix in ('.json','.dds','.png','.at9'):
                    items.append((name,path.read_bytes(),e['flags1']))
            check('recovered metadata '+row['title_id'],make_pkg(items),True,[x[0] for x in items])
(ROOT/'TEST_RESULTS.json').write_text(json.dumps({'passed':len(results),'cases':results,
    'scope':'Host execution of exact C PKG parser; synthetic tables; optional local fixtures are not included.'},indent=2),encoding='utf-8')
print(f'PASS: {len(results)} parser cases')
