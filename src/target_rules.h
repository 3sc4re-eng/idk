#pragma once
#include "entities.h"
#include "config.h"
namespace target_rules{
// Movement classifications come from the installed ability behavior flags.
inline bool movement(unsigned token){switch(token){case 2824119765u:case 509856396u:case 725153709u:case 147493976u:case 494112189u:case 1792773251u:case 1426567660u:case 1009029159u:case 2054144742u:case 1904698842u:case 2031714424u:case 3452399392u:case 2240607294u:case 3516947824u:case 1247583368u:case 1963397252u:case 2670099061u:case 2398133749u:case 1336069669u:case 1568845221u:case 1458044103u:case 1835738020u:case 2717130900u:case 1796564033u:case 1469918191u:case 4147641675u:case 2566573207u:return true;default:return false;}}
inline bool selected(const Entity& local,const Entity& enemy,const std::string& prefix){
    if(!enemy.alive||enemy.team==local.team||!enemy.visible||enemy.invuln||enemy.stealthed)return false;
    if(!cfg.B((prefix+"all_heroes").c_str())&&!cfg.B((prefix+"hero."+std::to_string(enemy.hero)).c_str()))return false;
    if(cfg.B((prefix+"soul_lead_only").c_str())&&(enemy.souls<local.souls||enemy.souls-local.souls<(u64)cfg.I((prefix+"soul_lead").c_str())))return false;
    if(cfg.B((prefix+"movement_ready").c_str())){
        if(!enemy.abilities_valid)return false;bool found=false;
        for(const auto& a:enemy.abilities)if(a.valid&&a.ready&&movement(a.token)&&cfg.B((prefix+"watch."+std::to_string(a.token)).c_str()))found=true;
        if(!found)return false;
    }return true;
}
}
