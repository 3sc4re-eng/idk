from pathlib import Path
import struct,json,re,hashlib
ROOT=Path(__file__).resolve().parent
class PE:
 def __init__(self,path):
  self.path=Path(path);self.data=self.path.read_bytes();b=self.data;self.pe=struct.unpack_from('<I',b,0x3c)[0];o=self.pe+24
  self.timestamp=struct.unpack_from('<I',b,self.pe+8)[0];self.size=struct.unpack_from('<I',b,o+56)[0];self.base=struct.unpack_from('<Q',b,o+24)[0];self.sections=[]
  n=struct.unpack_from('<H',b,self.pe+6)[0];opt=struct.unpack_from('<H',b,self.pe+20)[0]
  for i in range(n):
   p=o+opt+i*40;name=b[p:p+8].split(b'\0')[0].decode();vs,rva,rs,raw=struct.unpack_from('<4I',b,p+8);self.sections.append(dict(name=name,rva=rva,size=vs,raw=raw,raw_size=rs))
 def rva(self,raw):
  for s in self.sections:
   if s['raw']<=raw<s['raw']+s['raw_size']:return s['rva']+raw-s['raw']
  raise ValueError(raw)
 def raw(self,rva):
  for s in self.sections:
   if s['rva']<=rva<s['rva']+s['raw_size']:return s['raw']+rva-s['rva']
  raise ValueError(rva)
 def scan(self,pattern):
  expression=b''.join(b'.'if x in ['?','??']else re.escape(bytes([int(x,16)]))for x in pattern.split());hits=[]
  for s in self.sections:
   if s['name']!='.text':continue
   for m in re.finditer(expression,self.data[s['raw']:s['raw']+s['raw_size']],re.S):hits.append(self.rva(s['raw']+m.start()))
  return hits
 def rtti(self):
  b=self.data;descriptors={}
  for m in re.finditer(rb'\.\?AV([^\x00]{1,180})@@\x00',b):
   try:descriptors[self.rva(m.start()-16)]=m[1].decode()
   except (ValueError,UnicodeDecodeError):pass
  cols={}
  for s in self.sections:
   if s['name']!='.rdata':continue
   for p in range(s['raw'],s['raw']+s['raw_size']-24,4):
    sig,offset,cd,typ,desc,self_=struct.unpack_from('<6I',b,p)
    if sig==1 and offset==0 and typ in descriptors and self_==self.rva(p):cols[self.base+self_]=descriptors[typ]
  tables={}
  for s in self.sections:
   if s['name']!='.rdata':continue
   for p in range(s['raw'],s['raw']+s['raw_size']-8,8):
    pointer=struct.unpack_from('<Q',b,p)[0]
    if pointer in cols:tables[self.rva(p)+8]=cols[pointer]
  return tables

if __name__=='__main__':
 client=PE(r'F:\SteamLibrary\steamapps\common\Deadlock\game\citadel\bin\win64\client.dll');schema=json.loads((ROOT/'deadlock-schema.json').read_text());assert schema['revision']=='11193905'or schema['revision']==11193905
 classes={(c['module'],c['name']):c for c in schema['classes']};tables=client.rtti();(ROOT/'client-rtti.json').write_text(json.dumps({hex(k):v for k,v in tables.items()},indent=2));print('RTTI primary tables',len(tables))
 def inherits(name,base):
  if name==base:return True
  c=classes.get(('client',name));return bool(c)and any(inherits(p['name'],base)for p in c.get('parents',[])if p['module']=='client')
 def kind(name):
  if not inherits(name,'C_BaseEntity'):return 0
  if inherits(name,'C_CitadelPlayerPawn'):return 1
  if inherits(name,'CCitadelPlayerController'):return 2
  if name=='C_CitadelTeam':return 3
  if name=='C_World':return 8
  if inherits(name,'C_BaseDoor') or name=='C_Citadel_FissureWall':return 9
  if inherits(name,'C_CitadelBaseAbility'):return 4
  if inherits(name,'C_CitadelProjectile'):return 6
  if any(k in name for k in ['NPC','C_AI_','Trooper']):return 5
  roots=('C_Citadel_Pickup','C_CitadelItemPickup','C_Citadel_BreakableProp','C_BreakableProp','C_ItemCrate')
  if any(inherits(name,base) for base in roots):return 7
  return 0
 wanted=['C_BaseEntity','CEntityInstance','CGameSceneNode','CSkeletonInstance','CModelState','CBasePlayerController','CCitadelPlayerController','PlayerDataGlobal_t','C_BasePlayerPawn','CPlayer_ObserverServices','C_CitadelPlayerPawn','CCitadelAbilityComponent','C_CitadelBaseAbility','CModifierProperty','CCitadelHeroComponent','CitadelHeroSpawnData_t','C_CitadelGameRules','C_CitadelTeam','STeamFOWEntity','CCitadelBaseLockonAbility','LockonTarget_t','CCitadel_Ability_Lash_Ultimate','CCitadel_Ability_HoldMelee','CCitadel_Ability_UltCombo','CCitadel_Ability_Hornet_Snipe','C_CitadelProjectile','CCitadel_Ability_Bull_Leap','CCitadel_Ability_LashDownStrike','CCitadel_Ability_Bull_Charge','CCitadelAutoScaledTime']
 header='#pragma once\n#include <cstdint>\n// Exact installed revision 11193905; generated from SchemaExplorer and local PE analysis.\nnamespace dl_layout{\n'
 header+=f'inline constexpr uint32_t revision=11193905,client_timestamp={client.timestamp}u,client_image_size={client.size}u;\n'
 for name in wanted:
  matches=[c for (mod,n),c in classes.items()if n==name and mod in ['client','entity2']];assert len(matches)>=1,name;c=next((c for c in matches if c['module']=='client'),matches[0]);header+='namespace '+name+'{\n'
  header+=f'inline constexpr uint32_t size={c.get("size",0)};\n'
  for f in c.get('fields',[]):
   if f.get('type',{}).get('category')=='bitfield'or 'offset'not in f:continue
   header+=f'inline constexpr uint32_t {f["name"]}={f["offset"]};\n'
  header+='}\n'
 header+='struct Class{uint32_t vtable;const char* name;int kind;};\ninline constexpr Class classes[]={\n'
 for rva,name in sorted(tables.items()):
  k=kind(name)
  if k:header+='{'+str(rva)+','+json.dumps(name)+','+str(k)+'},\n'
 header+='};\n'
 proof=[]
 for key,pattern in [('entities','48 8B 1D ? ? ? ? 48 89 1D ? ? ? ? 4C 63 B3'),('view','48 8D 0D ? ? ? ? 48 C1 E0 06'),('rules','48 89 1D ? ? ? ? FF 15 ? ? ? ? 84 C0')]:
  hits=client.scan(pattern);assert hits,key;targets={r+7+struct.unpack_from('<i',client.data,client.raw(r)+3)[0]for r in hits};assert len(targets)==1,(key,targets)
  target=targets.pop();assert any(s['name']=='.data'and s['rva']<=target<s['rva']+s['size']for s in client.sections)
  header+=f'inline constexpr uint32_t {key}_instruction={hits[0]}, {key}_rva={target};\n';proof.append(dict(name=key,instructions=hits,target_rva=target,pattern=pattern))
 for module in ['engine2','inputsystem']:
  binary=PE(Path(r'F:\SteamLibrary\steamapps\common\Deadlock\game\bin\win64')/(module+'.dll'))
  header+=f'inline constexpr uint32_t {module}_timestamp={binary.timestamp}u,{module}_image_size={binary.size}u;\n'
  if module=='engine2':
   vt=next(rva for rva,n in binary.rtti().items()if n=='CEngineGameUI');section=next(s for s in binary.sections if s['name']=='.data');needle=struct.pack('<Q',binary.base+vt)
   hits=[binary.rva(section['raw']+i)for i in range(0,section['raw_size']-8,8)if binary.data[section['raw']+i:section['raw']+i+8]==needle];assert len(hits)==1
   header+=f'inline constexpr uint32_t engine_ui={hits[0]},engine_ui_vtable={vt},engine_ui_flags=0x60;\n'
  else:
   getter=binary.data[binary.raw(0x29d0):binary.raw(0x29d0)+8];assert getter[:3]==b'\x48\x8d\x05'and getter[-1]==0xc3
   instance=0x29d0+7+struct.unpack_from('<i',getter,3)[0];fn=binary.data[binary.raw(0x47a0):binary.raw(0x47a0)+32];j=fn.index(b'\x48\x8d\x0d');key_table=0x47a0+j+7+struct.unpack_from('<i',fn,j+3)[0]
   vt=next(rva for rva,n in binary.rtti().items()if n=='CInputSystem');header+=f'inline constexpr uint32_t input_instance={instance},input_vtable={vt},virtual_key_table={key_table},input_buttons=0x12b8;\n'
 header+='}\n';(ROOT/'deadlock_layout.h').write_text(header)
 (ROOT/'layout-proof.json').write_text(json.dumps(dict(revision=schema['revision'],schema_url='https://s2v.app/SchemaExplorer/assets/deadlock.json-DdBOyIt5.gz',client_sha256=hashlib.sha256(client.data).hexdigest(),client_timestamp=client.timestamp,image_size=client.size,globals=proof,known_classes=sum(bool(kind(n))for n in tables.values())),indent=2))
 for rva,name in tables.items():
  if name=='CCitadelInput':
   print('CCitadelInput vtable',hex(rva));p=client.raw(rva);print([(i,hex(struct.unpack_from('<Q',client.data,p+i*8)[0]-client.base))for i in range(30)])
 print('Generated exact layout and',len(proof),'validated RIP targets')
