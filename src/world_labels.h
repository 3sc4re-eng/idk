#pragma once
#include <array>
#include <string_view>

// Exact client entity classes checked against cached schema revision 11193905.
// Names do not imply model/loot variants that have not been read.
namespace world_labels{
enum class Icon{Pickup,Health,Currency,Crate,Breakable,Rejuvenator,Idol,Candy,Drawing};
struct Definition{std::string_view type;const char* label;Icon icon;};
inline constexpr std::array<Definition,20> definitions={{
    {"C_Citadel_BreakableProp","Breakable",Icon::Breakable},
    {"C_Citadel_BreakableProp_PunchableTroll","Punchable troll",Icon::Breakable},
    {"C_Citadel_BreakableProp_Hideout","Breakable",Icon::Breakable},
    {"C_BreakableProp","Breakable",Icon::Breakable},
    {"C_Citadel_Pickup","Pickup",Icon::Pickup},
    {"CCitadel_Pickup_HalloweenCandy","Halloween candy",Icon::Candy},
    {"C_Citadel_Pickup_Ability","Ability pickup",Icon::Pickup},
    {"C_Citadel_Pickup_Item","Item pickup",Icon::Pickup},
    {"C_Citadel_Pickup_Health","Health pickup",Icon::Health},
    {"C_Citadel_Pickup_Gold","Gold pickup",Icon::Currency},
    {"C_Citadel_Pickup_Currency","Currency pickup",Icon::Currency},
    {"C_Citadel_Pickup_AssignedCurrencies","Currency bundle",Icon::Currency},
    {"C_Citadel_Pickup_NecroDeath","Death pickup",Icon::Pickup},
    {"C_Citadel_Pickup_Modifier","Power-up pickup",Icon::Pickup},
    {"C_CitadelItemPickup","Item pickup",Icon::Pickup},
    {"CCitadelItemPickupRejuv","Rejuvenator",Icon::Rejuvenator},
    {"CCitadelItemPickupRejuvHeroTest","Rejuvenator",Icon::Rejuvenator},
    {"CCitadelItemPickupIdol","Idol",Icon::Idol},
    {"C_ItemCrate","Crate",Icon::Crate},
    {"CCitadel_Pickup_ArtistDrawing","Drawing",Icon::Drawing}
}};
inline constexpr const Definition* find(std::string_view type){for(const auto& value:definitions)if(value.type==type)return &value;return nullptr;}
inline constexpr bool displayable(std::string_view type){return find(type)!=nullptr;}
}
