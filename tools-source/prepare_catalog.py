from pathlib import Path
import json,re,struct
P=Path(__file__).resolve().parent
abilities=json.loads((P/'abilities.json').read_text());heroes=json.loads((P/'heroes.json').read_text());roster=json.loads((P.parent/'current_hero_catalog.json').read_text())
locale={}
for path in Path(r'F:\SteamLibrary\steamapps\common\Deadlock\game\citadel\resource\localization').rglob('*english.txt'):
 if 'generated_vo'in str(path)or 'patch_notes'in str(path):continue
 for k,v in re.findall(r'^\s*"([^"\n]+)"[^\S\n]+"((?:\\.|[^"\\\n])*)"',path.read_text(encoding='utf-8-sig'),re.M):locale[k.lower()]=v.replace('\\"','"')
def token(text):
 b=text.lower().encode();m=0x5bd1e995;h=0x31415926^len(b);i=0
 while i+4<=len(b):
  k=int.from_bytes(b[i:i+4],'little');k=k*m&0xffffffff;k^=k>>24;k=k*m&0xffffffff;h=(h*m&0xffffffff)^k;i+=4
 tail=b[i:]
 for n,v in enumerate(tail):h^=v<<(n*8)
 if tail:h=h*m&0xffffffff
 h^=h>>13;h=h*m&0xffffffff;h^=h>>15;return h
def number(v):
 try:
  text=str(v).strip().lower()
  if text.endswith('ms'):return float(text[:-2])/1000
  if text.endswith('s'):return float(text[:-1])
  return float(text[:-1])*39.37007874 if text.endswith('m')else float(text)
 except ValueError:return 0
def prop(v,key):return number(v.get('m_mapAbilityProperties',{}).get(key,{}).get('m_strValue',0))
entries=[]
def add(key,hero,slot,item=False):
 v=abilities.get(key);assert v,key;title=locale.get(key.lower());assert title,(key,'missing English title')
 enemy='ENEMY'in v.get('m_nAbilityTargetTypes','');behavior=v.get('m_AbilityBehaviorsBits','');projectiles=v.get('m_mapProjectileInfos',{});speed=max([float(p.get('m_flSpeed',0))for p in projectiles.values()]+[0]);radius=max([prop(v,k)for k in ['AbilityRadius','Radius','ExplosionRadius','ImpactRadius','DamageRadius','VacuumRadius','EndRadius','ExplodeRadius','SigilRadius']]+[0]);cast=prop(v,'AbilityCastDelay');channel=prop(v,'AbilityChannelTime');target_limit=prop(v,'AbilityUnitTargetLimit')
 aoe=enemy and radius>0 and ('NO_TARGET'in behavior or 'SPHERE'in v.get('m_eAbilityTargetingShape','')or target_limit>1)
 if key=='citadel_ability_lash_ultimate':aoe=True
 single=enemy and not aoe and target_limit<=1
 projectile_radius=max([float(p.get(k,0))for p in projectiles.values()for k in ['m_flTriggerRadius','m_flCapsuleTriggerRadius','m_flPhysicsRadius']]+[0])
 entry=dict(key=key,title=title,hero=hero,slot=slot,item=item,token=token(key),projectile=speed>0,speed=speed,radius=radius,cast=cast,channel=channel,aoe=aoe,single=single,enemy=enemy,passive='PASSIVE'in v.get('m_eAbilityActivation',''),charges=int(prop(v,'AbilityCharges')),start_trained=v.get('m_bStartTrained',False),self_centered=v.get('m_eAbilityTargetingLocation')=='CITADEL_ABILITY_TARGETING_LOCATION_SELF',projectile_radius=projectile_radius)
 entries.append(entry)
for hero in roster:
 v=heroes[hero['internal']]
 for slot in range(4):
  key=v.get('m_mapBoundAbilities',{}).get('ESlot_Signature_'+str(slot+1));assert key,hero['name'];add(key,hero['id'],slot)
for key,v in abilities.items():
 if not isinstance(v,dict)or v.get('m_eAbilityType')!='EAbilityType_Item':continue
 if key.startswith('upgrade_')and(v.get('m_eAbilityActivation')!='CITADEL_ABILITY_ACTIVATION_PASSIVE'or key in ['upgrade_counterspell','upgrade_active_reload'])and key.lower()in locale:add(key,0,-1,True)
add('citadel_ability_melee_parry',0,22)
assert len({e['token']for e in entries})==len(entries),'Ability token collision'
header='#pragma once\n#include <cstdint>\n// Current installed hero/ability data and English localization, revision 11193905.\nnamespace ability_data{\nstruct Entry{uint32_t token,hero;int slot;const char* key;const char* title;bool item,projectile,aoe,single,enemy,passive,start_trained,self_centered;int charges;float speed,radius,cast,channel,projectile_radius;};\ninline constexpr Entry entries[]={\n'
for e in entries:
 values=[str(e['token'])+'u',str(e['hero']),str(e['slot']),json.dumps(e['key']),json.dumps(e['title'])]+[str(e[k]).lower()for k in ['item','projectile','aoe','single','enemy','passive','start_trained','self_centered']]+[str(e['charges'])]+[str(float(e[k]))+'f'for k in ['speed','radius','cast','channel','projectile_radius']];header+='{'+','.join(values)+'},\n'
header+='};\ninline const Entry* find(uint32_t token){for(auto& e:entries)if(e.token==token)return &e;return nullptr;}\ninline const Entry* signature(unsigned hero,int slot){for(auto& e:entries)if(e.hero==hero&&e.slot==slot&&!e.item)return &e;return nullptr;}\n}\n'
(P/'ability_catalog.h').write_text(header,encoding='utf-8');(P/'ability-catalog.json').write_text(json.dumps(entries,indent=2))
print('Catalogue:',len(roster),'heroes;',sum(not e['item']for e in entries),'hero abilities;',sum(e['item']for e in entries),'items;',sum(e['projectile']for e in entries),'projectile definitions')
for hero in ['Apollo','Mina','Dynamo']:
 h=next(h for h in roster if h['name']==hero);print(hero,[(e['slot']+1,e['title'],e['key'])for e in entries if e['hero']==h['id']])
