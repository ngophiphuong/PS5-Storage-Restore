"""Build the same json-c 0.19 API as the PS5 SDK for Windows host tests."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEP = ROOT/'build/host-json-c019'
DEP.mkdir(parents=True,exist_ok=True)
URL='https://s3.amazonaws.com/json-c_releases/releases/json-c-0.19-nodoc.tar.gz'
SHA='704927172443309a8efeb162060bb215548e1286e5568514007dd2cc35a0a164'
archive=DEP/'json-c-0.19-nodoc.tar.gz'
if not archive.exists(): urllib.request.urlretrieve(URL,archive)
assert hashlib.sha256(archive.read_bytes()).hexdigest()==SHA,'json-c download hash mismatch'
if not (DEP/'json-c-0.19').exists():
    with tarfile.open(archive) as tf: tf.extractall(DEP,filter='data')
ninja=shutil.which('ninja') or str(ROOT/'build/host-deps/bin/ninja.exe')
assert Path(ninja).is_file(),'Install Ninja or run: python -m pip install --target build/host-deps ninja'
with (DEP/'build.log').open('w',encoding='utf-8') as log:
    subprocess.run(['cmake','-S',str(DEP/'json-c-0.19'),'-B',str(DEP/'compiled'),'-G','Ninja',
                    '-DCMAKE_MAKE_PROGRAM='+ninja,'-DCMAKE_C_COMPILER='+shutil.which('clang'),
                    '-DBUILD_SHARED_LIBS=OFF','-DBUILD_TESTING=OFF','-DBUILD_APPS=OFF'],check=True,stdout=log,stderr=subprocess.STDOUT)
    subprocess.run(['cmake','--build',str(DEP/'compiled'),'--target','json-c','--parallel','4'],check=True,stdout=log,stderr=subprocess.STDOUT)
(DEP/'source_manifest.json').write_text(json.dumps({'url':URL,'sha256':SHA,'version':'0.19'},indent=2),encoding='utf-8')
print('json-c 0.19 host library ready')
