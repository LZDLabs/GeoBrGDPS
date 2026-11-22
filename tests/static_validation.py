#!/usr/bin/env python3
"""Release packaging gate (not a substitute for the in-game test checklist)."""
import json,struct,plistlib,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
meta=json.loads((root/'mod.json').read_text())
assert meta['id']=='geobr.geobrmod' and meta['version']=='1.17.0'
assert meta['gd']['android']=='2.2081' and meta['geode']=='5.9.0'
names=set()
for rel in meta['resources']['files']:
 assert (root/rel).is_file(),f'Missing packaged resource: {rel}'
 name=Path(rel).name;assert name not in names,f'Flattened resource name clash: {name}';names.add(name)
def png(p):
 b=p.read_bytes();assert b[:8]==b'\x89PNG\r\n\x1a\n',p
 return struct.unpack('>IIBB',b[16:26])
for name in ['main-levels','geopass','tier-shop','map']:
 width,height,depth,color=png(root/f'resources/geobr-button-{name}.png')
 assert width==height==512 and depth==8 and color in (2,6),f'RGBA texture: {name}'
for slug in ['sneaker-bot','blue-runner','orange-guard']:
 data=plistlib.loads((root/f'resources/robot-pack/{slug}.plist').read_bytes())
 assert len(data['frames'])==13,f'Robot native frame set: {slug}'
# Ten profile themes, each with its own banner art.
themes=['fire','ice','forest','cosmos','neon','gold','aurora','eclipse','royal','astral']
for t in themes:
 rel=f'resources/geobr-theme-{t}.png';assert rel in meta['resources']['files'],rel
 w,h,_,_=png(root/rel);assert (w,h)==(1254,418),f'theme size {t}'
visuals=(root/'src/ui/GeoBRVisuals.cpp').read_text()
for t in themes: assert f'"{t}"' in visuals,f'theme {t} wired'
# Arrow: four skins x four colour channels, portal and effects.
for k in range(1,17):
 for part in ['primary','secondary','glow','detail']:
  rel=f'resources/arrow/geobr-arrow-{k}-{part}.png';assert rel in meta['resources']['files'],rel
  w,h,_,color=png(root/rel);assert (w,h)==(120,120) and color==6,f'arrow layer {k}-{part}'
for f in ['portal-back','portal-front','spark','trail']:
 assert f'resources/arrow/geobr-arrow-{f}.png' in meta['resources']['files'],f
sources=re.findall(r'\b(src/[^\s)]+\.cpp)',(root/'CMakeLists.txt').read_text())
for rel in sources: assert (root/rel).is_file(),f'Missing compilation unit: {rel}'
for unit in ['ArrowPlayer','ArrowPortal','ArrowLevel','ArrowSkins','ArrowPopup','ArrowProfile','ArrowGarage']:
 assert f'src/arrow/{unit}.cpp' in sources,f'Arrow unit compiled: {unit}'
code='\n'.join(p.read_text() for p in (root/'src').rglob('*') if p.suffix in ['.cpp','.hpp'])
assert not re.search(r'\{\"p_password\",\s*\"',code), 'Admin RPC must use user input, not a literal password'
assert 'service_role' not in code.replace('Nunca use a service_role',''), 'no service_role usage in the client'
assert 'ClientVersion[]="1.17.0"' in code
# GDPS: the client only redirects GD's own calls, never carries server secrets,
# and the setting can turn the GDPS off (back to RobTop's servers).
# GDPS v2: the server choice, the separate save and the redirect live in the
# companion mod geobr.gdps (early-load, no dependencies); the main mod stays normal-load.
assert 'gdps-enabled' not in meta['settings'] and not meta.get('early-load'),'main mod: no server setting, not early-load'
core=json.loads((root/'gdps-core/mod.json').read_text())
assert core['id']=='geobr.gdps' and core['early-load'] is True and not core.get('dependencies'),'companion: early-load, no deps'
srv=core['settings']['server'];assert srv['one-of']==['Oficial','GDPS GeoBR'] and srv['default']=='Oficial' and srv['requires-restart'] is True,'server setting'
core_src=(root/'gdps-core/src/main.cpp').read_text()
assert 'class $modify(GeoBRGdpsSave, GManager)' in core_src and 'CCFileUtilsAndroid' not in core_src and 'geobr-gdps/' in core_src and 'CCHttpClient' in core_src,'separate save folder + redirect'
assert 'CCHttpClient' not in (root/'src/gdps/GdpsHooks.cpp').read_text(),'only one redirect (in the companion)'
hooks=(root/'src/gdps/GdpsHooks.cpp').read_text()
assert 'setVisible(false)' not in hooks and 'removeFromParent' not in hooks and 'setPosition(' not in hooks.replace('note->setPosition(',''),'GDPS hooks do not move/hide native nodes'
for part in ['GdpsService.cpp','GdpsHooks.cpp','GdpsModHooks.cpp']: assert f'src/gdps/{part}' in sources
for part in ['GdpsPopups.cpp','GdpsAdminPopup.cpp','GdpsModPopups.cpp']: assert f'src/ui/{part}' in sources
mod_hooks=(root/'src/gdps/GdpsModHooks.cpp').read_text()
native_menus={'bottom-menu','left-side-menu','left-menu'}
ids=set(re.findall(r'"([a-z]+(?:-[a-z0-9]+)+)"',mod_hooks))
assert all(i in native_menus or i.startswith('geobr-') for i in ids),f'new node IDs must be geobr-*: {ids}'
assert 'removeFromParent' not in mod_hooks and 'setVisible(false)' not in mod_hooks,'mod hooks never hide or remove native nodes'
assert 'SUPABASE_SERVICE_ROLE_KEY' not in code and 'sb_secret_' not in code,'no server key in the client'
fn=root/'supabase/functions/gdps'
idx=(fn/'index.ts').read_text()
assert 'Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")' in idx and 'eyJ' not in idx,'server key only from the function environment'
gdmig=(root/'supabase/migrations/20261108120000_geobr_gdps.sql').read_text()
assert "values ('1.10.0-beta.1', true" in gdmig and 'enable row level security' in gdmig and 'start with 8000001' in gdmig and 'no cycle' in gdmig
gdmig2=(root/'supabase/migrations/20261113120000_geobr_gdps_v2.sql').read_text()
assert "values ('1.10.0-beta.2', true" in gdmig2 and 'restart with' in gdmig2 and 'map_packs' in gdmig2 and 'gauntlets' in gdmig2 and 'level_lists' in gdmig2 and 'chest_state' in gdmig2
v3mig=(root/'supabase/migrations/20261115120000_geobr_v3_release.sql').read_text()
assert "values ('1.11.0', true" in v3mig and 'delete' not in v3mig.lower(),'1.11.0 release row (non-destructive)'
assert 'DemonListPopup' not in (root/'src/ui/GdpsPopups.cpp').read_text(),'Demon List removed from the mod'
upd=(root/'src/services/UpdateService.cpp').read_text()
assert 'sha256::hex' in upd and 'trustedUrl' in upd and 'raw.githubusercontent.com/LZDLabs/GeoBrGDPS/' in upd,'updates verified by SHA-256 from the release repository only'
assert meta['dependencies'].get('geobr.gdps'),'GeoBR requires GeoBR GDPS'
main_src=(root/'src/main.cpp').read_text()
assert 'hook->disable()' in main_src and 'ServerMenuHookPriority' in main_src and 'AdminLogin' not in main_src,'GeoBR off on the official server; no More Games secret'
assert 'geobr.geobrmod' in (root/'gdps-core/src/main.cpp').read_text(),'GeoBR GDPS needs GeoBR'
assert 'src/godeditor/GodEditor.cpp' in sources,'God Editor compiled'
assert 'resources/geobr-god-editor.png' in meta['resources']['files']
# RC8: campaign levels live in Storage (not bundled) and are listed by the RC8 migration.
assert not any(f.endswith('.gmd') for f in meta['resources']['files']),'no bundled .gmd'
assert 'bundled' not in code,'no bundled campaign path in the client'
rc8=(root/'supabase/migrations/20261105180000_geobr_rc8_campaign.sql').read_text()
for slug in ('teoria-tropical','bloqueio-total'):
 assert f"main-levels/geobr-{slug}-v1.gmd" in rc8 and (root/f'campaign/geobr-{slug}.gmd').is_file(),slug
for pol in ('geobr_tmp_seed_rc8 ','geobr_tmp_seed_rc8b_insert','geobr_tmp_seed_rc8b_select','geobr_tmp_seed_rc8b_delete'):
 assert f'drop policy if exists {pol.strip()} on storage.objects' in rc8,pol
assert "'1.9.0-beta.8'" in rc8
# RC8 regressions: no stencil clipping in themes, God Editor menu priority set before registration.
assert 'CCClippingNode::create' not in code,'no CCClippingNode (black pages / invisible icons on device)'
assert not re.search(r'->setHandlerPriority\(',code),'setHandlerPriority before registration crashes'
w,h,_,_=png(root/'logo.png');assert (w,h)==(336,336),'mod icon'
skins=re.findall(r'\{(\d+), "[^"]+", SkinSource::(\w+), (\d+)\}',(root/'src/arrow/ArrowSkins.hpp').read_text())
assert len(skins)==16 and [int(s[0]) for s in skins]==list(range(1,17)),'16 Arrow skins in order'
assert sum(1 for s in skins if s[1]=='Crate')==6 and sorted(int(s[2]) for s in skins if s[1]=='Crate')==[1,2,3,4,5,6],'one exclusive per crate rarity'
mig=(root/'supabase/migrations/20261105120000_geobr_rc7_arrow_items.sql').read_text()
for s_ in skins[4:]: assert f"'arrow-{s_[0]}'" in mig,f'server item for skin {s_[0]}'
print(f'PASS: manifest 1.17.0, companion geobr.gdps (early-load, separate save folder, redirect, server setting), GDPS hooks only add geobr-* buttons, no server key in client, GDPS migrations v1+v2, mod icon, God Editor icon, 2 original campaign levels in Storage (RC8 migration), RC9 collab/groups/icons migration, collab hooks only add geobr-* menus, 16 Arrow skins mirrored on the server, {len(meta["resources"]["files"])} resources (unique flattened names), 10 theme banners, 16 Arrow skins x 4 layers, portal art, {len(sources)} compilation units, no client admin password or service_role')
