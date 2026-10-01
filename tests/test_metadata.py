"""Host regression tests of the production streaming/backup metadata functions."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
DEP = ROOT / 'build/host-json-c019'
INCLUDE = DEP / 'include/json-c'
INCLUDE.mkdir(parents=True, exist_ok=True)
for p in (DEP / 'json-c-0.19').glob('*.h'):
    shutil.copyfile(p, INCLUDE / p.name)
for name in ['json_config.h', 'json.h']:
    p = DEP / 'compiled' / name
    if p.exists(): shutil.copyfile(p, INCLUDE / name)
EXE = ROOT / 'build/metadata_probe.exe'
subprocess.run([shutil.which('clang'), '-O2', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-function', '-I', str(INCLUDE.parent),
                str(ROOT / 'tests/metadata_probe.c'), str(DEP / 'compiled/libjson-c.a'),
                '-o', str(EXE)], check=True)
EVIDENCE = ROOT / 'tests/fixtures/metadata'
results = []

def digest(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()

with tempfile.TemporaryDirectory(prefix='metadata-test-', dir=ROOT / 'build') as td:
    base = Path(td)
    def case(name, mode, canonical, previous, success, replace=False, blocked_backup=False):
        folder = base / str(len(results)); folder.mkdir()
        src = folder / 'package-param.json'; dst = folder / 'existing-param.json'
        src.write_bytes(canonical)
        if previous is not None: dst.write_bytes(previous)
        if blocked_backup:
            (folder / 'state').mkdir(); (folder / 'state/backups').write_bytes(b'occupied')
        p = subprocess.run([str(EXE), mode, str(src), str(dst)], cwd=folder, capture_output=True, text=True)
        assert (p.returncode == 0) == success, (name, p.returncode, p.stdout, p.stderr, (folder/'restore.log').read_text())
        if success and mode != 'preflight': assert dst.read_bytes() == canonical, name
        elif previous is not None: assert dst.read_bytes() == previous, name
        else: assert not dst.exists(), name
        backups = list((folder/'state/backups').glob('*.param.json')) if (folder/'state/backups').is_dir() else []
        if replace:
            assert len(backups) == 1 and backups[0].read_bytes() == previous, name
        elif success:
            assert not backups, name
        assert not list(folder.glob('*.tmp')), name
        if success and mode != 'preflight':
            again = subprocess.run([str(EXE), 'param', str(src), str(dst)], cwd=folder, capture_output=True, text=True)
            assert again.returncode == 0 and 'updated=0' in again.stdout and 'created=0' in again.stdout, name
        results.append({'case': name, 'result': p.stdout.strip(), 'passed': True})
    ff = (EVIDENCE/'PPSA08668/package_param.json').read_bytes()
    ff_old = (EVIDENCE/'PPSA08668/source_param.json').read_bytes()
    sp = (EVIDENCE/'PPSA01467/package_param.json').read_bytes()
    sp_old = (EVIDENCE/'PPSA01467/appmeta_param.json').read_bytes()
    case('synthetic stale creationDate repaired with exact backup', 'param', ff, ff_old, True, replace=True)
    case('synthetic stale SDK metadata repaired with exact backup', 'param', sp, sp_old, True, replace=True)
    case('missing param created', 'param', sp, None, True)
    case('identical param unchanged', 'param', ff, ff, True)
    case('preflight makes no changes', 'preflight', ff, ff_old, True)
    for key, value in [('titleId','PPSA99999'), ('contentId','DIFFERENT_CONTENT'), ('contentVersion','99.000.000')]:
        obj = json.loads(ff); obj[key] = value
        case('reject different '+key, 'param', ff, json.dumps(obj).encode(), False)
    case('reject invalid existing JSON', 'param', ff, b'{invalid', False)
    case('reject trailing non-JSON bytes', 'param', ff+b' garbage', ff_old, False)
    case('rename failure preserves old and backup', 'fail-rename', ff, ff_old, False, replace=True)
    case('backup failure preserves old', 'param', ff, ff_old, False, blocked_backup=True)
    obj=json.loads(ff); obj['streamingTest']='x'*(2*1024*1024+13)
    case('param larger than 1 MiB parsed across chunks', 'param', json.dumps(obj).encode(), ff_old, True, replace=True)
    # Real streamed data spanning 1,040 chunks and the former 32/64 MiB caps.
    folder=base/'large'; folder.mkdir(); src=folder/'input.bin'; dst=folder/'output.bin'
    block=bytes(range(256))*256
    with src.open('wb') as f:
        for _ in range(1040): f.write(block)
        f.write(b'last-partial-block')
    p=subprocess.run([str(EXE),'blob',str(src),str(dst)],cwd=folder,capture_output=True,text=True)
    assert p.returncode==0,(p.stdout,p.stderr)
    assert dst.stat().st_size==src.stat().st_size and digest(dst)==digest(src)
    results.append({'case':'stream 65 MiB plus final partial chunk', 'bytes':src.stat().st_size,'sha256':digest(dst),'passed':True})
(ROOT/'METADATA_TEST_RESULTS_1.3.json').write_text(json.dumps({'passed':len(results),'cases':results,
    'scope':'Production C functions + json-c 0.19, original synthetic metadata; Windows regular-file POSIX rename shim. No console mount/launch.'},indent=2),encoding='utf-8')
print('PASS:',len(results),'metadata regression cases')
