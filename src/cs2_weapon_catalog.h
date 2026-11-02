#pragma once
#include <array>
#include <cstdint>
#include <string_view>
// IDs/keys/types from installed scripts/items/items_game.txt; see asset-index-selected.json.
namespace cs2_weapons {
struct Weapon {std::uint16_t definition_index;const char* key;const char* label;const char* category;};
inline constexpr std::array<Weapon,34> catalog{{
{1,"weapon_deagle","Desert Eagle","Pistols"},
{2,"weapon_elite","Dual Berettas","Pistols"},
{3,"weapon_fiveseven","Five-SeveN","Pistols"},
{4,"weapon_glock","Glock-18","Pistols"},
{7,"weapon_ak47","AK-47","Rifles"},
{8,"weapon_aug","AUG","Rifles"},
{9,"weapon_awp","AWP","Snipers"},
{10,"weapon_famas","FAMAS","Rifles"},
{11,"weapon_g3sg1","G3SG1","Snipers"},
{13,"weapon_galilar","Galil AR","Rifles"},
{14,"weapon_m249","M249","Machine guns"},
{16,"weapon_m4a1","M4A4","Rifles"},
{17,"weapon_mac10","MAC-10","SMGs"},
{19,"weapon_p90","P90","SMGs"},
{23,"weapon_mp5sd","MP5-SD","SMGs"},
{24,"weapon_ump45","UMP-45","SMGs"},
{25,"weapon_xm1014","XM1014","Shotguns"},
{26,"weapon_bizon","PP-Bizon","SMGs"},
{27,"weapon_mag7","MAG-7","Shotguns"},
{28,"weapon_negev","Negev","Machine guns"},
{29,"weapon_sawedoff","Sawed-Off","Shotguns"},
{30,"weapon_tec9","Tec-9","Pistols"},
{32,"weapon_hkp2000","P2000","Pistols"},
{33,"weapon_mp7","MP7","SMGs"},
{34,"weapon_mp9","MP9","SMGs"},
{35,"weapon_nova","Nova","Shotguns"},
{36,"weapon_p250","P250","Pistols"},
{38,"weapon_scar20","SCAR-20","Snipers"},
{39,"weapon_sg556","SG 553","Rifles"},
{40,"weapon_ssg08","SSG 08","Snipers"},
{60,"weapon_m4a1_silencer","M4A1-S","Rifles"},
{61,"weapon_usp_silencer","USP-S","Pistols"},
{63,"weapon_cz75a","CZ75-Auto","Pistols"},
{64,"weapon_revolver","R8 Revolver","Pistols"},
}};
inline constexpr const Weapon* find(std::uint16_t index) noexcept {for(const auto& weapon:catalog)if(weapon.definition_index==index)return &weapon;return nullptr;}
inline constexpr const Weapon* find(std::string_view key) noexcept {for(const auto& weapon:catalog)if(key==weapon.key)return &weapon;return nullptr;}
static_assert(find(7)&&std::string_view(find(7)->key)=="weapon_ak47");
static_assert(!find(0)&&!find(65535));
}
