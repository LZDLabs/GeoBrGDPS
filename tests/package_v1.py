import zipfile,json,struct,sys
from pathlib import Path
p=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(p/'tests'))
from elf_needed import needed,undeclared_mod_libs
f=Path(sys.argv[1]) if len(sys.argv)>1 else p/'dist/geobr.geobrmod.geode'
meta=json.loads((p/'mod.json').read_text())
with zipfile.ZipFile(f) as z:
 assert z.testzip() is None
 m=json.loads(z.read('mod.json'));assert m['version']=='1.11.0'
 so=[n for n in z.namelist() if n.endswith('.so')];assert len(so)==1 and so[0]=='geobr.geobrmod.android64.so'
 b=z.read(so[0]);assert b[:4]==b'\x7fELF' and b[4]==2 and struct.unpack('<H',b[18:20])[0]==183
 bad=undeclared_mod_libs(b,m.get('dependencies'));assert not bad,f'.so needs libraries of undeclared mods: {bad}'
 for rel in meta['resources']['files']:
  name='resources/geobr.geobrmod/'+Path(rel).name;assert name in z.namelist(),f'missing {name}'
  assert z.read(name)==(p/rel).read_bytes(),f'content differs {name}'
 assert z.read('logo.png')==(p/'logo.png').read_bytes(),'mod icon packaged'
 print(f'PASS: intact package, mod icon, manifest 1.11.0, AArch64 ELF, NEEDED only system/Geode/declared mods ({needed(b)}), all {len(meta["resources"]["files"])} resources byte-identical under resources/geobr.geobrmod/')
